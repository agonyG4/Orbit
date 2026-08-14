use astrea_filechooser_portal::{
    DialogMode, DialogRunConfig, PortalError, PortalOptions, PortalSelection, default_run_config,
    run_dialog_with_config, save_file_names_from_options, uris_for_save_files, uris_from_selection,
};
use std::collections::HashMap;
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

#[zbus::interface(name = "org.freedesktop.impl.portal.Request")]
impl PortalRequest {
    async fn close(&self) -> zbus::fdo::Result<()> {
        let _ = self.cancel.send(true);
        Ok(())
    }
}

struct AstreaFileChooser {
    dialog_slots: Arc<Semaphore>,
    run_config: DialogRunConfig,
}

impl AstreaFileChooser {
    fn new() -> Self {
        Self {
            dialog_slots: Arc::new(Semaphore::new(MAX_CONCURRENT_DIALOGS)),
            run_config: default_run_config(),
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

        let result =
            run_dialog_with_config(mode, &title, &options, &self.run_config, cancellation).await;
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
    let _connection = connection::Builder::session()?
        .name(BUS_NAME)?
        .serve_at(OBJECT_PATH, AstreaFileChooser::new())?
        .build()
        .await?;
    std::future::pending::<()>().await;
    Ok(())
}
