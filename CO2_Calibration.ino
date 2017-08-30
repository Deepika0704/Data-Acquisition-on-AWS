// Grove - Co2 Sensor calibration

//#include <SoftwareSerial.h>
//SoftwareSerial sensor(A5, A4);      // TX, RX
void(* resetFunc) (void) = 0;
#define sensor Serial1

const unsigned char cmd_calibrate[] = 
{
    0xff, 0x87, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf2
};

void setup()
{
    sensor.begin(9600);
    Serial.begin(115200);
    Serial.println("begin to calibrate");

    for(int i=0; i<sizeof(cmd_calibrate); i++)
    {
        sensor.write(cmd_calibrate[i]);
    }

    Serial.println("calibrate done");
}

void loop()
{
    Serial.println("waiting to reset");
    resetFunc();
    delay(2000);
    Serial.println("Reset Done");
    // nothing to do
    
}
