mod arduino;
mod firmware;
mod memory;

fn main() {
    match arduino::dispatch() {
        Ok(true) => return,
        Ok(false) => {}
        Err(error) => {
            eprintln!("stcxx Arduino adapter: {error:#}");
            std::process::exit(stcxx_driver::process_exit_code(&error));
        }
    }
    stcxx_driver::main_entry();
}
