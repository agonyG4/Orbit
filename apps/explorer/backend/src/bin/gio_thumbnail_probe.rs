use gio::prelude::*;
use std::env;
use std::path::PathBuf;

fn main() {
    let Some(source) = env::args().nth(1).map(PathBuf::from) else {
        std::process::exit(2);
    };
    let Ok(info) = gio::File::for_path(source).query_info(
        "thumbnail::path-normal,thumbnail::is-valid-normal",
        gio::FileQueryInfoFlags::NONE,
        None::<&gio::Cancellable>,
    ) else {
        std::process::exit(1);
    };
    if !info.boolean("thumbnail::is-valid-normal") {
        std::process::exit(1);
    }
    let Some(path) = info.attribute_byte_string("thumbnail::path-normal") else {
        std::process::exit(1);
    };
    println!("{}", path.as_str());
}
