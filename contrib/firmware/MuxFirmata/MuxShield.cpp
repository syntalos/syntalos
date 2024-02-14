/*
 MuxShield.cpp - Library for using Mayhew Labs' Mux Shield.
 Created by Mark Mayhew, December 29, 2012.
 Updated by Mark Mayhew, May 22, 2016: Fixed digital output issue where erroneous data was being clocked into shift registers
 Released into the public domain.
 */

#include "Arduino.h"
#include "MuxShield.h"

int _shiftReg1[16]={0};
int _shiftReg2[16]={0};
int _shiftReg3[16]={0};

bool _IO1IsOutput = 0;
bool _IO2IsOutput = 0;
bool _IO3IsOutput = 0;

MuxShield::MuxShield(int S0, int S1, int S2, int S3, int OUTMD,int IOS1, int IOS2, int IOS3, int IO1, int IO2, int IO3)
{
    _S0 = S0;
    _S1 = S1;
    _S2 = S2;
    _S3 = S3;
    _OUTMD = OUTMD;
    _IOS1 = IOS1;
    _IOS2 = IOS2;
    _IOS3 = IOS3;
    _IO1 = IO1;
    _IO2 = IO2;
    _IO3 = IO3;

    pinMode(_S0,OUTPUT);
    pinMode(_S1,OUTPUT);
    pinMode(_S2,OUTPUT);
    pinMode(_S3,OUTPUT);
    pinMode(_OUTMD,OUTPUT);
    digitalWrite(_OUTMD,LOW);
    pinMode(_IOS1,OUTPUT);
    pinMode(_IOS2,OUTPUT);
    pinMode(_IOS3,OUTPUT);
}

MuxShield::MuxShield()
{
    _S0 = 2;
    _S1 = 4;
    _S2 = 6;
    _S3 = 7;
    _OUTMD = 8;
    _IOS1 = 10;
    _IOS2 = 11;
    _IOS3 = 12;
    _IO1 = A0;
    _IO2 = A1;
    _IO3 = A2;

    pinMode(_S0,OUTPUT);
    pinMode(_S1,OUTPUT);
    pinMode(_S2,OUTPUT);
    pinMode(_S3,OUTPUT);
    pinMode(_OUTMD,OUTPUT);
    digitalWrite(_OUTMD,LOW);
    pinMode(_IOS1,OUTPUT);
    pinMode(_IOS2,OUTPUT);
    pinMode(_IOS3,OUTPUT);


}

void MuxShield::setMode(int mux, int mode)
{
    switch (mux) {
        case 0:
            switch (mode) {
                case DIGITAL_IN:
                    pinMode(_IO1,INPUT);
                    digitalWrite(_IOS1,LOW);
					_IO1IsOutput = 0;
                    break;
                case DIGITAL_IN_PULLUP:
                    pinMode(_IO1,INPUT_PULLUP);
                    digitalWrite(_IOS1,LOW);
					_IO1IsOutput = 0;
                    break;
                case DIGITAL_OUT:
                    pinMode(_IO1,OUTPUT);
                    digitalWrite(_IOS1,HIGH);
					_IO1IsOutput = 1;
                    break;
                case ANALOG_IN:
                    digitalWrite(_IOS1,LOW);
					_IO1IsOutput = 0;
                    break;
                default:
                    break;
            }
            break;
        case 1:
            switch (mode) {
                case DIGITAL_IN:
                    pinMode(_IO2,INPUT);
                    digitalWrite(_IOS2,LOW);
					_IO2IsOutput = 0;
                    break;
                case DIGITAL_IN_PULLUP:
                    pinMode(_IO2,INPUT_PULLUP);
                    digitalWrite(_IOS2,LOW);
					_IO2IsOutput = 0;
                    break;
                case DIGITAL_OUT:
                    pinMode(_IO2,OUTPUT);
                    digitalWrite(_IOS2,HIGH);
					_IO2IsOutput = 1;
                    break;
                case ANALOG_IN:
                    digitalWrite(_IOS2,LOW);
					_IO2IsOutput = 0;
                    break;
                default:
                    break;
            }
            break;
        case 2:
            switch (mode) {
                case DIGITAL_IN:
                    pinMode(_IO3,INPUT);
                    digitalWrite(_IOS3,LOW);
					_IO3IsOutput = 0;
                    break;
                case DIGITAL_IN_PULLUP:
                    pinMode(_IO3,INPUT_PULLUP);
                    digitalWrite(_IOS3,LOW);
					_IO3IsOutput = 0;
                    break;
                case DIGITAL_OUT:
                    pinMode(_IO3,OUTPUT);
                    digitalWrite(_IOS3,HIGH);
					_IO3IsOutput = 1;
                    break;
                case ANALOG_IN:
                    digitalWrite(_IOS3,LOW);
					_IO3IsOutput = 0;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

}

void MuxShield::digitalWriteMS(int mux, int chan, int val)
{
    int i;

    digitalWrite(_S3,LOW);                              //S3 here is LCLK
    digitalWrite(_OUTMD,HIGH);                          //set to output mode
    switch (mux) {
        case 0:
            _shiftReg1[chan] = val;                     //store value until updated again
			break;
        case 1:
            _shiftReg2[chan] = val;                     //store value until updated again
			break;
		case 2:
            _shiftReg3[chan] = val;                     //store value until updated again
			break;
		default:
            break;
	}

	if (_IO1IsOutput) {
		for (i=15; i>=0; i--) {
			digitalWrite(_S0,LOW);                  //S0 here is i/o1 _sclk
			digitalWrite(_IO1,_shiftReg1[i]);       //put value
			digitalWrite(_S0,HIGH);                 //latch in value
		}
	}

	if (_IO2IsOutput) {
		for (i=15; i>=0; i--) {
			digitalWrite(_S1,LOW);                  //S0 here is i/o2 _sclk
			digitalWrite(_IO2,_shiftReg2[i]);       //put value
			digitalWrite(_S1,HIGH);                 //latch in value
		}
	}

	if (_IO3IsOutput) {
		for (i=15; i>=0; i--) {
			digitalWrite(_S2,LOW);                  //S2 here is i/o3 _sclk
			digitalWrite(_IO3,_shiftReg3[i]);       //put value
			digitalWrite(_S2,HIGH);                 //latch in value
		}
	}

    digitalWrite(_S3,HIGH);                     //latch in ALL values
    digitalWrite(_OUTMD,LOW);                   //Exit output mode
}

int MuxShield::digitalReadMS(int mux, int chan)
{
    digitalWrite(_OUTMD,LOW);   //Set outmode off (i.e. set as input mode)
    int val;
    switch (mux) {
        case 0:
            digitalWrite(_S0, (chan&1));
            digitalWrite(_S1, (chan&3)>>1);
            digitalWrite(_S2, (chan&7)>>2);
            digitalWrite(_S3, (chan&15)>>3);

            val = digitalRead(_IO1);
            break;
        case 1:
            digitalWrite(_S0, (chan&1));
            digitalWrite(_S1, (chan&3)>>1);
            digitalWrite(_S2, (chan&7)>>2);
            digitalWrite(_S3, (chan&15)>>3);

            val = digitalRead(_IO2);
            break;
        case 2:
            digitalWrite(_S0, (chan&1));
            digitalWrite(_S1, (chan&3)>>1);
            digitalWrite(_S2, (chan&7)>>2);
            digitalWrite(_S3, (chan&15)>>3);

            val = digitalRead(_IO3);
            break;
        default:
            break;
    }
    return val;
}

int MuxShield::analogReadMS(int mux, int chan)
{
    digitalWrite(_OUTMD,LOW);
    int val;

    digitalWrite(_S0, (chan&1));
    digitalWrite(_S1, (chan&3)>>1);
    digitalWrite(_S2, (chan&7)>>2);
    digitalWrite(_S3, (chan&15)>>3);

    switch (mux) {
        case 0:
            val = analogRead(_IO1);
            break;
        case 1:
            val = analogRead(_IO2);
            break;
        case 2:
            val = analogRead(_IO3);
            break;
        default:
            break;
    }
    return val;
}
