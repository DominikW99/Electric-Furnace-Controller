# Electric furnance controller project
## Architecture
This project consists of 3 modules:
- The Main module, which controls the electric furnace based on data from the database
- Remote Temperature Measurement module that takes temperature measurements in a particular room
- Mobile app, which is used to set controller modes and desired temperatures

## Tests
The tests folder contains a set of automated tests that check the response of the main module to direct parameter changes in the database. The module's response is sent via the UART interface and is compared with the expected values.
