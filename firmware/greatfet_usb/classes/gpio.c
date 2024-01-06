/*
 * This file is part of GreatFET
 *
 * GPIO functions
 */

#include <drivers/comms.h>
#include <drivers/gpio.h>
#include <debug.h>

#include "../pin_manager.h"

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <ctype.h>

// This file's class number.
#define CLASS_NUMBER_SELF (0x103)

// Convenience type to cast between a pin configuration register and its 32 bit value.
typedef union {
	gpio_pin_configuration_t reg;
	uint32_t bits;
} pin_configuration_t;


static int gpio_verb_set_up_gpio_pin(struct command_transaction *trans)
{
	/**
	 * Deprecated: prefer gpio_verb_configure_gpio_pin
	 */
	
	gpio_pin_t gpio_pin;

	int rc;
	uint8_t as_output, initial_value, scu_group, scu_pin;

	gpio_pin.port  = comms_argument_parse_uint8_t(trans);
	gpio_pin.pin   = comms_argument_parse_uint8_t(trans);
	as_output      = comms_argument_parse_uint8_t(trans);
	initial_value  = comms_argument_parse_uint8_t(trans);

	if (!comms_transaction_okay(trans)) {
		return EBADMSG;
	}

	// First, ensure we can access the given pin.
	scu_group = gpio_get_group_number(gpio_pin);
	scu_pin   = gpio_get_pin_number(gpio_pin);

	// If this port/pin doesn't correspond to a valid physical pin,
	// fail out.
	if ((scu_group == 0xff) || (scu_pin == 0xff)) {
		return EINVAL;
	}

	// If we can't get a hold on the given pin.
	if (!pin_ensure_reservation(scu_group, scu_pin, CLASS_NUMBER_SELF)) {
		pr_warning("gpio: couldn't reserve busy pin GPIO%d[%d]!\n", gpio_pin.port, gpio_pin.pin);
		return EBUSY;
	}

	// Configure the pin to be used as a GPIO in the SCU.
	rc = gpio_configure_pinmux(gpio_pin);
	if (rc) {
		pr_warning("couldn't configure pinmux for GPIO%d[%d]\n", gpio_pin.port, gpio_pin.pin);
		return rc;
	}

	// If this is an output pin, set its direction.
	if (as_output) {
		rc = gpio_set_pin_value(gpio_pin, initial_value);
		if (rc) {
			pr_warning("couldn't set initial value for GPIO%d[%d]\n", gpio_pin.port, gpio_pin.pin);
			return rc;
		}
	}

	// Set the initial direction of the port pin.
	rc = gpio_set_pin_direction(gpio_pin, as_output);
	if (rc) {
		pr_warning("couldn't set initial direction for GPIO%d[%d]\n", gpio_pin.port, gpio_pin.pin);
		return rc;
	}

	// TODO: other properties here? pulls? drive? slew?
	// or should these be the job of the pin manager? (prolly not)

	return 0;
}


static int gpio_verb_configure_gpio_pin(struct command_transaction *trans)
{
	gpio_pin_t gpio_pin;
	uint8_t as_output, initial_value;
	pin_configuration_t configuration;

	uint8_t scu_group, scu_pin;
	int rc;
	
	gpio_pin_configuration_t scu_pin_configuration;
	
	gpio_pin.port      = comms_argument_parse_uint8_t(trans);
	gpio_pin.pin       = comms_argument_parse_uint8_t(trans);
	as_output          = comms_argument_parse_uint8_t(trans);
	initial_value      = comms_argument_parse_uint8_t(trans);
	configuration.bits = comms_argument_parse_uint32_t(trans);

	if (!comms_transaction_okay(trans)) {
		return EBADMSG;
	}

	// First, ensure we can access the given pin.
	scu_group = gpio_get_group_number(gpio_pin);
	scu_pin   = gpio_get_pin_number(gpio_pin);

	// If this port/pin doesn't correspond to a valid physical pin,
	// fail out.
	if ((scu_group == 0xff) || (scu_pin == 0xff)) {
		return EINVAL;
	}

	// If we can't get a hold on the given pin.
	if (!pin_ensure_reservation(scu_group, scu_pin, CLASS_NUMBER_SELF)) {
		pr_warning("gpio: couldn't reserve busy pin GPIO%d[%d]!\n", gpio_pin.port, gpio_pin.pin);
		return EBUSY;
	}

	// Set the pin's SCU configuration.
	if (as_output) {
		// Set the pin's slew rate.
		scu_pin_configuration.use_fast_slew = configuration.reg.use_fast_slew;

		// If the given pin is a high-drive pin, set the drive strength.
		scu_pin_configuration.drive_strength = configuration.reg.drive_strength;
		
	} else {
		// Set the pin's resistor mode.
		scu_pin_configuration.pull_resistors = configuration.reg.pull_resistors;

		// Set the pin's input buffer mode.
		scu_pin_configuration.input_buffer_enabled = configuration.reg.input_buffer_enabled;

		// Set the pin's glitch_filter mode.
		scu_pin_configuration.disable_glitch_filter = configuration.reg.disable_glitch_filter;
	}

	// Configure the pin to be used as a GPIO in the SCU.
	rc = gpio_configure_pinmux_and_pin(gpio_pin, scu_pin_configuration);
	if (rc) {
		pr_warning("couldn't configure pinmux for GPIO%d[%d]\n", gpio_pin.port, gpio_pin.pin);
		return rc;
	}

	// If this is an output pin, set its initial value.
	if (as_output) {
		rc = gpio_set_pin_value(gpio_pin, initial_value);
		if (rc) {
			pr_warning("couldn't set initial value for GPIO%d[%d]\n", gpio_pin.port, gpio_pin.pin);
			return rc;
		}
	}

	// Set the initial direction of the port pin.
	rc = gpio_set_pin_direction(gpio_pin, as_output);
	if (rc) {
		pr_warning("couldn't set initial direction for GPIO%d[%d]\n", gpio_pin.port, gpio_pin.pin);
		return rc;
	}
			
	return 0;
}


static int gpio_verb_release_pin(struct command_transaction *trans)
{
	uint32_t pin_owner;
	uint8_t scu_group, scu_pin;
	gpio_pin_t gpio_pin;

	gpio_pin.port = comms_argument_parse_uint8_t(trans);
	gpio_pin.pin  = comms_argument_parse_uint8_t(trans);

	if (!comms_transaction_okay(trans)) {
		return EBADMSG;
	}

	// Ensure we had access to the given pin.
	scu_group = gpio_get_group_number(gpio_pin);
	scu_pin   = gpio_get_pin_number(gpio_pin);
	pin_owner = pin_get_owning_class(scu_group, scu_pin);

	if (pin_owner != CLASS_NUMBER_SELF) {
		pr_error("gpio: tried to release a pin we didn't own (GPIO%d[%d] is owned by %" PRIu32 ")!",
				gpio_pin.port, gpio_pin.pin, pin_owner);
		return EINVAL;
	}

	// Place the input pin back into a high-Z state by disconnecting
	// its output driver.  TODO: should this be done in the SCU, as well?
	gpio_set_pin_direction(gpio_pin, false);

	// Finally, release the relevant reservation.
	return pin_release_reservation(scu_group, scu_pin);
}


static int gpio_verb_read_pins(struct command_transaction *trans)
{
	uint8_t port, pin, value;

	// Continue for as long as we have pins to handle.
	while (comms_argument_data_remaining(trans)) {

		// Parse the current port/pin pair...
		port = comms_argument_parse_uint8_t(trans);
		pin  = comms_argument_parse_uint8_t(trans);

		if (!comms_transaction_okay(trans)) {
			return EBADMSG;
		}

		value = gpio_get_pin_value(gpio_pin(port, pin));
		comms_response_add_uint8_t(trans, value);
	}

	return 0;
}


static int gpio_verb_get_pin_directions(struct command_transaction *trans)
{
	uint8_t port, pin, value;

	// Continue for as long as we have pins to handle.
	while (comms_argument_data_remaining(trans)) {

		// Parse the current port/pin pair...
		port = comms_argument_parse_uint8_t(trans);
		pin  = comms_argument_parse_uint8_t(trans);

		if (!comms_transaction_okay(trans)) {
			return EBADMSG;
		}

		value = gpio_get_pin_direction(gpio_pin(port, pin));
		comms_response_add_uint8_t(trans, value);
	}

	return 0;
}


static int gpio_verb_get_pin_configurations(struct command_transaction *trans)
{
    uint8_t scu_group, scu_pin;
    gpio_pin_t gpio_pin;

	platform_scu_registers_t *scu;	
	pin_configuration_t value;
	
    // Continue for as long as we have pins to handle.
    while (comms_argument_data_remaining(trans)) {

		// Parse the current port/pin pair...
		gpio_pin.port = comms_argument_parse_uint8_t(trans);
		gpio_pin.pin  = comms_argument_parse_uint8_t(trans);

		if (!comms_transaction_okay(trans)) {
			return EBADMSG;
		}

		// First, ensure we can access the given pin.
		scu_group = gpio_get_group_number(gpio_pin);
		scu_pin = gpio_get_pin_number(gpio_pin);

		// If this port/pin doesn't correspond to a valid physical pin,
		// fail out.
		if ((scu_group == 0xff) || (scu_pin == 0xff)) {
			return EINVAL;
		}

		// Read the scu pin configuration register block
		scu = platform_get_scu_registers();
		value.reg = scu->group[scu_group].pin[scu_pin];

		comms_response_add_uint32_t(trans, value.bits);
    }

    return 0;
}


static int gpio_verb_write_pins(struct command_transaction *trans)
{
	int rc;
	uint8_t port, pin, value;

	// Continue for as long as we have pins to handle.
	while (comms_argument_data_remaining(trans)) {

		// Parse the current port/pin pair...
		port  = comms_argument_parse_uint8_t(trans);
		pin   = comms_argument_parse_uint8_t(trans);
		value = comms_argument_parse_uint8_t(trans);

		if (!comms_transaction_okay(trans)) {
			return EBADMSG;
		}

		rc = gpio_set_pin_value(gpio_pin(port, pin), value);
		if (rc) {
			pr_error("gpio: could not assign to GPIO%d[%d] (%d)", port, pin, rc);
		}
	}

	return 0;
}


/**
 * Verbs for the firmware API.
 */
static struct comms_verb _verbs[] = {

		/* Configuration. */
		{  .name = "configure_pin", .handler = gpio_verb_configure_gpio_pin, .in_signature = "<BBBBI",
		   .out_signature = "", .in_param_names = "port, pin, as_output, initial_value, configuration",
		   .doc = "Configures a single pin to be used for GPIO." },
		{  .name = "release_pin", .handler = gpio_verb_release_pin, .in_signature = "<BB",
		   .out_signature = "", .in_param_names = "port, pin",
		   .doc = "Releases a GPIO pin for use by other peripherals." },
		{  .name = "get_pin_directions", .handler = gpio_verb_get_pin_directions, .in_signature = "<*(BB)",
		   .out_signature = "<*B", .in_param_names = "pins", .out_param_names = "directions",
		   .doc = "Reads the direction of a GPIO pin or pins given tuples of (port, pin)."
			   "Returns 1 for output; 0 for input." },
		{  .name = "get_pin_configurations", .handler = gpio_verb_get_pin_configurations, .in_signature = "<*(BB)",
		   .out_signature = "<*I", .in_param_names = "pins", .out_param_names = "configurations",
		   .doc = "Reads the SCU pin configuration registers of a GPIO pin or pins given tuples of (port, pin)."
			  "Returns the 32 bit SCU configuration register block for each pin." },

		/* Pin access functions. */
		{  .name = "read_pins", .handler = gpio_verb_read_pins, .in_signature = "<*(BB)",
		   .out_signature = "<*B", .in_param_names = "pins", .out_param_names = "values",
		   .doc = "Reads the value of a GPIO pin or pins given tuples of (port, pin)." },
		{  .name = "write_pins", .handler = gpio_verb_write_pins, .in_signature = "<*(BBB)",
		   .out_signature = "", .in_param_names = "pin_value_tuples",
		   .doc = "Sets the value of a GPIO pin or pins, given tuples of (port, pin, values)." },


		/* TODO: Port access functions. */

		/* Deprecated verbs. */
		{  .name = "set_up_pin", .handler = gpio_verb_set_up_gpio_pin, .in_signature = "<BBBB",
		   .out_signature = "", .in_param_names = "port, pin, as_output, initial_value",
		   .doc = "gpio.set_up_pin is deprecated; prefer configure_pin." },

		/* Sentinel. */
		{}
};
COMMS_DEFINE_SIMPLE_CLASS(gpio, CLASS_NUMBER_SELF, "gpio", _verbs,
		"API for simple GPIO manipulations.");
