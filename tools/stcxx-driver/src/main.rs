mod arduino;
mod firmware;

fn main() {
    match arduino::dispatch() {
        Ok(true) => return,
        Ok(false) => {}
        Err(error) => {
            eprintln!("stcxx Arduino adapter: {error:#}");
            std::process::exit(2);
        }
    }
    stcxx_driver::main_entry();
}
