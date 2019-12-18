/*******************************************************************************
 * This file is part of PsxNewLib.                                             *
 *                                                                             *
 * Copyright (C) 2019 by SukkoPera <software@sukkology.net>                    *
 *                                                                             *
 * PsxNewLib is free software: you can redistribute it and/or                  *
 * modify it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or           *
 * (at your option) any later version.                                         *
 *                                                                             *
 * PsxNewLib is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with PsxNewLib. If not, see http://www.gnu.org/licenses.              *
 *******************************************************************************
 *
 * This sketch shows how the library can be used to turn a PSX controller into
 * an USB one, using an Arduino Leonardo or Micro and the excellent
 * ArduinoJoystickLibrary.
 *
 * For details on ArduinoJoystickLibrary, see
 * https://github.com/MHeironimus/ArduinoJoystickLibrary.
 */

#include <PsxControllerBitBang.h>
#include <Joystick.h>

/* We must use the bit-banging interface, as SPI pins are only available on the
 * ICSP header on the Leonardo.
 *
 * Note that we use pins 9-12 so that 13 can be used with the built-in LED.
 */
const byte PIN_PS2_ATT = 9;
const byte PIN_PS2_CMD = 10;
const byte PIN_PS2_DAT = 11;
const byte PIN_PS2_CLK = 12;

PsxControllerBitBang<PIN_PS2_ATT, PIN_PS2_CMD, PIN_PS2_DAT, PIN_PS2_CLK> psx;

Joystick_ usbStick (
	JOYSTICK_DEFAULT_REPORT_ID,
	JOYSTICK_TYPE_MULTI_AXIS,
	12,			// buttonCount
	1,			// hatSwitchCount (0-2)
	true,		// includeXAxis
	true,		// includeYAxis
	false,		// includeZAxis
	true,		// includeRxAxis
	true,		// includeRyAxis
	false,		// includeRzAxis
	false,		// includeRudder
	false,		// includeThrottle
	false,		// includeAccelerator
	false,		// includeBrake
	false		// includeBrake
);

boolean haveController = false;


#define	toDegrees(rad) (rad * 180.0 / PI)

/** \brief Analog sticks idle value
 * 
 * Value reported when the analog stick is in the (ideal) center position.
 */
const byte ANALOG_IDLE_VALUE = 128U;

/** \brief Dead zone for analog sticks
 *  
 * If the analog stick moves less than this value from the center position, it
 * is considered still.
 * 
 * \sa ANALOG_IDLE_VALUE
 */
const byte ANALOG_DEAD_ZONE = 50U;


void setup () {
	// Lit the builtin led whenever buttons are pressed
	pinMode (LED_BUILTIN, OUTPUT);

	// Init Joystick library
	usbStick.begin ();
	usbStick.setXAxisRange (0, 255);
	usbStick.setYAxisRange (0, 255);
	usbStick.setRxAxisRange (0, 255);
	usbStick.setRyAxisRange (0, 255);

	Serial.begin (115200);
	Serial.println (F("Ready!"));
}

void loop () {
	if (!haveController) {
		if (psx.begin ()) {
			Serial.println (F("Controller found!"));
			delay (300);
			if (!psx.enterConfigMode ()) {
				Serial.println (F("Cannot enter config mode"));
			} else {
				// Try to enable analog sticks
				if (!psx.enableAnalogSticks ()) {
					Serial.println (F("Cannot enable analog sticks"));
				}
								
				if (!psx.exitConfigMode ()) {
					Serial.println (F("Cannot exit config mode"));
				}
			}
			
			haveController = true;
		}
	} else {
		if (!psx.read ()) {
			Serial.println (F("Controller lost :("));
			haveController = false;
		} else {
			byte x, y;
			
			// Flash led with buttons, I like this
			digitalWrite (LED_BUILTIN, !!psx.getButtonWord ());

			// Read was successful, so let's make up data for Joystick

			// Buttons first!
			usbStick.setButton (0, psx.buttonPressed (PSB_SQUARE));
			usbStick.setButton (1, psx.buttonPressed (PSB_CROSS));
			usbStick.setButton (2, psx.buttonPressed (PSB_CIRCLE));
			usbStick.setButton (3, psx.buttonPressed (PSB_TRIANGLE));
			usbStick.setButton (4, psx.buttonPressed (PSB_L1));
			usbStick.setButton (5, psx.buttonPressed (PSB_R1));
			usbStick.setButton (6, psx.buttonPressed (PSB_L2));
			usbStick.setButton (7, psx.buttonPressed (PSB_R2));
			usbStick.setButton (8, psx.buttonPressed (PSB_SELECT));
			usbStick.setButton (9, psx.buttonPressed (PSB_START));
			usbStick.setButton (10, psx.buttonPressed (PSB_L3));		// Only available on DualSchock and later controllers
			usbStick.setButton (11, psx.buttonPressed (PSB_R3));		// Ditto

			// D-Pad makes up the X/Y axes
			if (psx.buttonPressed (PSB_PAD_UP)) {
				usbStick.setYAxis (0);
			} else if (psx.buttonPressed (PSB_PAD_DOWN)) {
				usbStick.setYAxis (255);
			} else {
				usbStick.setYAxis (128);
			}
			
			if (psx.buttonPressed (PSB_PAD_LEFT)) {
				usbStick.setXAxis (0);
			} else if (psx.buttonPressed (PSB_PAD_RIGHT)) {
				usbStick.setXAxis (255);
			} else {
				usbStick.setXAxis (128);
			}

			// Left analog gets mapped to the X/Y rotation axes
			if (psx.getLeftAnalog (x, y)) {
				usbStick.setRxAxis (x);
				usbStick.setRyAxis (y);
			}


			// Right analog is the hat switch
			if (psx.getRightAnalog (x, y)){
				int8_t rx = 0, ry = 0;
				
				int8_t deltaRX = x - ANALOG_IDLE_VALUE;	// [-128 ... 127]
				uint8_t deltaRXabs = abs (deltaRX);
				if (deltaRXabs > ANALOG_DEAD_ZONE) {
					rx = deltaRX;
					if (rx == -128)
						rx = -127;
				}
				
				int8_t deltaRY = y - ANALOG_IDLE_VALUE;
				uint8_t deltaRYabs = abs (deltaRY);
				if (deltaRYabs > ANALOG_DEAD_ZONE) {
					ry = deltaRY;
					if (ry == -128)
						ry = -127;
				}

				if (rx == 0 && ry == 0) {
					usbStick.setHatSwitch (0, JOYSTICK_HATSWITCH_RELEASE);
				} else {
					float angle = atan2 (-ry, -rx) + 2 * PI - PI / 2;
					usbStick.setHatSwitch (0, (uint16_t) toDegrees (angle));
				}
			}
		}
	}
	
	delay (1000 / 60);
}
