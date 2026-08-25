use astrea_filechooser_portal::{
    DialogMode, DialogRunConfig, PortalError, PortalOptions, PortalSelection, default_run_config,
    run_dialog_with_config, save_file_names_from_options, uris_for_save_files, uris_from_selection,
};
use std::collections::HashMap;
use std::future::Future;
use std::pin::Pin;
use std::sync::Arc;
use tokio::sync::{Semaphore, watch};
use zbus::connection;
use zbus::object_server::ObjectServer;
use zvariant::{OwnedObjectPath, OwnedValue, Value};

const BUS_NAME: &str = "org.freedesktop.impl.portal.desktop.astrea";
const OBJECT_PATH: &str = "/org/freedesktop/portal/desktop";
const RESPONSE_SUCCESS: u32 = 0;
const RESPONSE_CANCELLED: u32 = 1;
const MAX_CONCURRENT_DIALOGS: usize = 4;

type PortalResponse = (u32, HashMap<String, OwnedValue>);

struct PortalRequest {
    cancel: watch::Sender<bool>,
    #[allow(dead_code)]
    app_id: String,
    #[allow(dead_code)]
    parent_window: String,
}

type DialogFuture<'a> =
    Pin<Box<dyn Future<Output = Result<PortalSelection, PortalError>> + Send + 'a>>;

trait DialogRunner: Send + Sync {
    fn run<'a>(
        &'a self,
        mode: DialogMode,
        title: &'a str,
        options: &'a PortalOptions,
        cancellation: watch::Receiver<bool>,
    ) -> DialogFuture<'a>;
}

struct ProcessDialogRunner {
    run_config: DialogRunConfig,
}

impl DialogRunner for ProcessDialogRunner {
    fn run<'a>(
        &'a self,
        mode: DialogMode,
        title: &'a str,
        options: &'a PortalOptions,
        cancellation: watch::Receiver<bool>,
    ) -> DialogFuture<'a> {
        Box::pin(run_dialog_with_config(
            mode,
            title,
            options,
            &self.run_config,
            cancellation,
        ))
    }
}

#[zbus::interface(name = "org.freedesktop.impl.portal.Request")]
impl PortalRequest {
    async fn close(&self) -> zbus::fdo::Result<()> {
        let _ = self.cancel.send(true);
        Ok(())
    }
}

struct AstreaFileChooser {
    dialog_slots: Arc<Semaphore>,
    dialog_runner: Arc<dyn DialogRunner>,
    shutdown: watch::Receiver<bool>,
    _shutdown_sender: Option<watch::Sender<bool>>,
}

impl AstreaFileChooser {
    #[cfg(test)]
    fn with_runner(dialog_runner: Arc<dyn DialogRunner>) -> Self {
        let (shutdown_sender, shutdown) = watch::channel(false);
        Self::with_runner_and_sender(dialog_runner, shutdown, Some(shutdown_sender))
    }

    fn with_runner_and_shutdown(
        dialog_runner: Arc<dyn DialogRunner>,
        shutdown: watch::Receiver<bool>,
    ) -> Self {
        Self::with_runner_and_sender(dialog_runner, shutdown, None)
    }

    fn with_runner_and_sender(
        dialog_runner: Arc<dyn DialogRunner>,
        shutdown: watch::Receiver<bool>,
        shutdown_sender: Option<watch::Sender<bool>>,
    ) -> Self {
        Self {
            dialog_slots: Arc::new(Semaphore::new(MAX_CONCURRENT_DIALOGS)),
            dialog_runner,
            shutdown,
            _shutdown_sender: shutdown_sender,
        }
    }

    async fn run_request(
        &self,
        object_server: &ObjectServer,
        handle: OwnedObjectPath,
        app_id: String,
        parent_window: String,
        mode: DialogMode,
        title: String,
        options: PortalOptions,
    ) -> zbus::fdo::Result<Result<PortalSelection, PortalError>> {
        let slot = self.dialog_slots.try_acquire().map_err(|_| {
            zbus::fdo::Error::Failed("too many concurrent file dialogs".to_string())
        })?;
        let (cancel, cancellation) = watch::channel(false);
        if *self.shutdown.borrow() {
            return Err(zbus::fdo::Error::Failed(
                "portal is shutting down".to_string(),
            ));
        }
        let mut shutdown = self.shutdown.clone();
        let cancel_for_shutdown = cancel.clone();
        object_server
            .at(
                handle.clone(),
                PortalRequest {
                    cancel,
                    app_id,
                    parent_window,
                },
            )
            .await
            .map_err(|error| zbus::fdo::Error::Failed(error.to_string()))?;

        let mut dialog = Box::pin(self.dialog_runner.run(mode, &title, &options, cancellation));
        let result = tokio::select! {
            result = &mut dialog => result,
            changed = shutdown.changed() => {
                if changed.is_ok() && *shutdown.borrow() {
                    let _ = cancel_for_shutdown.send(true);
                    dialog.await
                } else {
                    Err(PortalError::new("portal shutdown state failed"))
                }
            }
        };
        let _ = object_server.remove::<PortalRequest, _>(handle).await;
        drop(slot);
        Ok(result)
    }
}

#[zbus::interface(name = "org.freedesktop.impl.portal.FileChooser")]
impl AstreaFileChooser {
    #[zbus(name = "OpenFile")]
    #[zbus(out_args("response", "results"))]
    async fn open_file(
        &self,
        #[zbus(object_server)] object_server: &ObjectServer,
        handle: OwnedObjectPath,
        app_id: String,
        parent_window: String,
        title: String,
        options: PortalOptions,
    ) -> zbus::fdo::Result<(u32, HashMap<String, OwnedValue>)> {
        let selection = self
            .run_request(
                object_server,
                handle,
                app_id,
                parent_window,
                DialogMode::OpenFile,
                title,
                options,
            )
            .await?;
        Ok(response_from_selection(selection))
    }

    #[zbus(name = "SaveFile")]
    #[zbus(out_args("response", "results"))]
    async fn save_file(
        &self,
        #[zbus(object_server)] object_server: &ObjectServer,
        handle: OwnedObjectPath,
        app_id: String,
        parent_window: String,
        title: String,
        options: PortalOptions,
    ) -> zbus::fdo::Result<(u32, HashMap<String, OwnedValue>)> {
        let selection = self
            .run_request(
                object_server,
                handle,
                app_id,
                parent_window,
                DialogMode::SaveFile,
                title,
                options,
            )
            .await?;
        Ok(response_from_selection(selection))
    }

    #[zbus(name = "SaveFiles")]
    #[zbus(out_args("response", "results"))]
    async fn save_files(
        &self,
        #[zbus(object_server)] object_server: &ObjectServer,
        handle: OwnedObjectPath,
        app_id: String,
        parent_window: String,
        title: String,
        options: PortalOptions,
    ) -> zbus::fdo::Result<(u32, HashMap<String, OwnedValue>)> {
        let selection = self
            .run_request(
                object_server,
                handle,
                app_id,
                parent_window,
                DialogMode::SelectFolder,
                title,
                options.clone(),
            )
            .await?;
        let selection = match selection {
            Ok(selection) if selection.accepted => selection,
            Ok(_) | Err(_) => return Ok(cancelled_response()),
        };
        let file_names = save_file_names_from_options(&options);
        match uris_for_save_files(&selection, &file_names) {
            Ok(uris) => Ok(success_response(uris)),
            Err(_) => Ok(cancelled_response()),
        }
    }
}

fn response_from_selection(
    selection: Result<PortalSelection, impl std::fmt::Display>,
) -> PortalResponse {
    match selection {
        Ok(selection) if selection.accepted => match uris_from_selection(&selection) {
            Ok(uris) => success_response(uris),
            Err(_) => cancelled_response(),
        },
        Ok(_) | Err(_) => cancelled_response(),
    }
}

fn success_response(uris: Vec<String>) -> PortalResponse {
    let mut results = HashMap::new();
    if let Ok(value) = OwnedValue::try_from(Value::from(uris)) {
        results.insert("uris".to_string(), value);
    }
    (RESPONSE_SUCCESS, results)
}

fn cancelled_response() -> PortalResponse {
    (RESPONSE_CANCELLED, HashMap::new())
}

#[tokio::main(flavor = "current_thread")]
async fn main() -> zbus::Result<()> {
    let (shutdown, shutdown_receiver) = watch::channel(false);
    let _connection = connection::Builder::session()?
        .name(BUS_NAME)?
        .serve_at(OBJECT_PATH, {
            let run_config = default_run_config();
            AstreaFileChooser::with_runner_and_shutdown(
                Arc::new(ProcessDialogRunner { run_config }),
                shutdown_receiver,
            )
        })?
        .build()
        .await?;
    tokio::select! {
        _ = tokio::signal::ctrl_c() => {
            let _ = shutdown.send(true);
            tokio::time::sleep(std::time::Duration::from_millis(100)).await;
        }
        _ = std::future::pending::<()>() => {}
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicUsize, Ordering};
    use tokio::time::{Duration, sleep, timeout};
    use zbus::Proxy;

    struct FakeDialogRunner {
        calls: AtomicUsize,
    }

    impl DialogRunner for FakeDialogRunner {
        fn run<'a>(
            &'a self,
            _mode: DialogMode,
            _title: &'a str,
            _options: &'a PortalOptions,
            mut cancellation: watch::Receiver<bool>,
        ) -> DialogFuture<'a> {
            let call = self.calls.fetch_add(1, Ordering::SeqCst);
            Box::pin(async move {
                if call == 0 {
                    if !*cancellation.borrow() {
                        let _ = cancellation.changed().await;
                    }
                    return Err(PortalError::new("dialog cancelled"));
                }
                Ok(PortalSelection {
                    accepted: true,
                    file_path: Some("/tmp/fake-dialog.txt".to_string()),
                    ..PortalSelection::default()
                })
            })
        }
    }

    async fn close_request(connection: &zbus::Connection, path: &str) -> zbus::Result<()> {
        let proxy = Proxy::new(
            connection,
            BUS_NAME,
            path,
            "org.freedesktop.impl.portal.Request",
        )
        .await?;
        proxy.call("Close", &()).await
    }

    #[tokio::test(flavor = "current_thread")]
    async fn private_dbus_request_lifecycle() -> zbus::Result<()> {
        let runner = Arc::new(FakeDialogRunner {
            calls: AtomicUsize::new(0),
        });
        let connection = connection::Builder::session()?
            .name(BUS_NAME)?
            .serve_at(OBJECT_PATH, AstreaFileChooser::with_runner(runner))?
            .build()
            .await?;
        let chooser = Proxy::new(
            &connection,
            BUS_NAME,
            OBJECT_PATH,
            "org.freedesktop.impl.portal.FileChooser",
        )
        .await?;

        let first_path = "/org/freedesktop/portal/desktop/request/first";
        let first_handle = OwnedObjectPath::try_from(first_path).unwrap();
        let first_args = (
            first_handle,
            "org.astrea.Test".to_string(),
            "".to_string(),
            "Open".to_string(),
            HashMap::<String, OwnedValue>::new(),
        );
        let first_chooser = chooser.clone();
        let first_call = tokio::spawn(async move {
            let response: PortalResponse = first_chooser.call("OpenFile", &first_args).await?;
            Ok::<PortalResponse, zbus::Error>(response)
        });

        let _close_result = timeout(Duration::from_secs(2), async {
            loop {
                match close_request(&connection, first_path).await {
                    Ok(()) => break,
                    Err(_) => sleep(Duration::from_millis(10)).await,
                }
            }
        })
        .await
        .expect("request must be exported");
        let first_response = first_call.await.unwrap()?;
        assert_eq!(first_response.0, RESPONSE_CANCELLED);
        assert!(close_request(&connection, first_path).await.is_err());

        let second_path = "/org/freedesktop/portal/desktop/request/second";
        let second_args = (
            OwnedObjectPath::try_from(second_path).unwrap(),
            "org.astrea.Test".to_string(),
            "".to_string(),
            "Open".to_string(),
            HashMap::<String, OwnedValue>::new(),
        );
        let second_response: PortalResponse = chooser.call("OpenFile", &second_args).await?;
        assert_eq!(second_response.0, RESPONSE_SUCCESS);
        assert!(second_response.1.contains_key("uris"));
        assert!(close_request(&connection, second_path).await.is_err());
        Ok(())
    }
}
