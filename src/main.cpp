#include "driver/Driver.hpp"

int main(int argc, char *argv[]) {
    DMZ::CompilerOptions options = DMZ::CompilerOptions::parse_arguments(argc, argv);

    DMZ::Driver& driver = DMZ::Driver::create_instance(options);
    return driver.main();
}