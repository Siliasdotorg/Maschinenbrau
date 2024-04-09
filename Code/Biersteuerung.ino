#include <OneWire.h>
#include <DallasTemperature.h>

// GPIO Pins Definieren ----------------------------------------------------------------------------
const int oneWireBus = 4;     
const int plate = 25;

const int pump = 26;
const int pumpChannel = 1;
const int pumpFreq = 8000;
const int pwmRes = 8;
int pumpValue = 0;

// Variablen die für die Prozessregelung gebraucht werden definieren ------------------------------
int timeRef = 0;
double now;
double ellapsedTime = 0;
float setT;
float curT;
float tempWuerze;
float tempRohr;
float heatingWuerze;
float heatGradWuerze = 0;
float heatingRohr;
float heatGradRohr = 0;

// Variablen für die Prozesssteuerung definieren -------------------------------------------------
int brauStatus = 01; //In welchem Schritt er sich befindet: Ganze 10er während einem Schritt, wenn noch ne 1 hintendran ist heizt er grade zur nächsten Stufe

// Einmaischen Definieren
float t_einmaischen = 5; //Zeit in Minuten
int T_einmaischen = 50; //Temperatur in Grad Celsius
// Rasten Definieren
int t_rast_1 = 10; //Zeit in Minuten
int T_rast_1 = 60; //Temperatur in Grad Celsius

// Sensoren Aufsetzen ----------------------------------------------------------------------------
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);
// Number of temperature devices found
int numberOfDevices;
// We'll use this variable to store a found device address
DeviceAddress tempDeviceAddress; 


// Funktionen definieren -----------------------------------------------------------------------------------------------------------------------------
// Der Regler 


// Setup ---------------------------------------------------------------------------------------------------------------------------------------
void setup() {
  //Alle Zeiten in Millisekunden umrechnen damit ein direkter vergleich mit millis() möglich ist
  t_einmaischen = t_einmaischen*1000*60; 
  t_rast_1 = t_rast_1*1000*60;

  // Start the Serial Monitor
  Serial.begin(115200);
  // Start the DS18B20 sensor -----------------------------------------------------------
  sensors.begin();
  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();
  
  // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" devices.");

  for(int i=0;i<numberOfDevices; i++){
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i)){
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      //sensors.setResolution(tempDeviceAddress, 12);
      
      Serial.println();
    } else {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
    }
  }
  
  
  pinMode(plate,OUTPUT);
  pinMode(pump,OUTPUT);
  
  Serial.println("Setup complete");
}

// Loop ---------------------------------------------------------------------------------------------------------------------------------------
void loop() {
  now = millis(); //Aktualisiert den Relativen Zeitpunkt in Bezug auf Anfang des Programms. 

// Verarbeitung der Temperaturdaten ---------------------------------------------------------------------------------------------------------------------------------------
  // Temperatur Daten ziehen
  sensors.requestTemperatures(); // Send the command to get temperatures
  
  // Loop through each device, print out temperature data
  for(int i=0;i<numberOfDevices; i++){
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i)){
      
      float tempC = sensors.getTempC(tempDeviceAddress);

      if (i == 0) {
        heatingWuerze = tempC-tempWuerze;
        heatingWuerze = heatingWuerze * 60;
        heatGradWuerze = 0.9*heatGradWuerze + 0.1*heatingWuerze;
        tempWuerze = tempC;
      }
      if (i == 1) {
        heatingRohr = tempC-tempRohr;
        heatGradRohr = 0.9*heatGradRohr + 0.1*(heatingRohr * 60);
        tempRohr = tempC;
      }
    }
  }
  // Serial Printout
  Serial.println(" Temperaturen: ---------------------");
  Serial.print(" Würze: ");
  Serial.print(tempWuerze);
  Serial.print(" °C; ");
  Serial.print(" Malzrohr: ");
  Serial.print(tempRohr);
  Serial.println(" °C; ");
  Serial.print(" dT Würze: ");
  Serial.print(heatGradWuerze);
  Serial.print(" °C/min; ");
  Serial.print(" dT Malzrohr: ");
  Serial.print(heatGradRohr);
  Serial.println(" °C/min; ");
  
  Serial.println(" Laufzeit: ---------------------");
  Serial.print(" ");
  Serial.print(ellapsedTime);
  Serial.println(" ms ");

  curT = tempRohr;
  
// Setzen des Braustatus ---------------------------------------------------------------------------------------------------------------------------------------
  if ( brauStatus == 1 ) {
    setT = T_einmaischen;
    if ((setT - curT) < 2) {
      brauStatus = 10;
      timeRef = now;
      }
    }
    else if (brauStatus == 10) {
    setT = T_einmaischen;
    if (now > timeRef + t_einmaischen) {
        brauStatus = 11;  
        }
    }
    else if (brauStatus == 11) {
    setT = T_rast_1;
    if ((setT - curT) < 2) {
      brauStatus = 20;
      timeRef = now;
      }
    }
    else if (brauStatus == 20) {
    setT = T_rast_1;
    if (now > timeRef + t_rast_1) {
        brauStatus = 21;
        }
    }
  else {
    setT = 0;
  }
  
 regler(setT,curT,heatGradRohr);
 //Serial.println(brauStatus);
 //pumpenSteuerung();
 
 
 Serial.println(" ");
 Serial.println(" ");

 delay(1000-(millis() - now));
 ellapsedTime = millis() - now;
}

// Funktion um die ganze Regelaction zu behausen
void regler(float setT,float curT,float tempGradient) {
  float tDif = setT - curT;
  
  Serial.println(" Regler: ---------------------");
  Serial.print(" setT:");
  Serial.print(setT);
  Serial.print(",");
  Serial.print("curT:");
  Serial.print(curT);
  Serial.print(",");
  Serial.print("tDif:");
  Serial.print(tDif);
  Serial.print(",State: ");
  
   if (tDif > 1.5 && tempGradient <= 1) { // tDif 25 ist so groß, das kommt eig. nur beim Einheizen fürs Einmaischen und Kochen vor
      digitalWrite(plate, HIGH); // Kein Limiter für runAvg zu setzen hilft dem System sich zu stabiliseren bevor der Regler aktiv wird
      Serial.println("On");
   }
   else if (tDif < 1 || tempGradient > 1){
      digitalWrite(plate, LOW);
      Serial.println("Off");
  }
}

// Funktion um die Pumpe zu regulieren
void pumpenSteuerung() {
  
  pumpValue = (pumpValue + 10) % 255; // Insgesamt 256 Stufen von 3,3V -> 25 ca. 0,3V Schritte
  pumpValue = 100;
  //ledcWrite(pumpChannel, pumpValue); //
  analogWrite(pump,pumpValue);

  
  Serial.println(" Pumpe: ---------------------");
  Serial.println(pumpValue);

}

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++){
    if (deviceAddress[i] < 16) Serial.print("0");
      Serial.print(deviceAddress[i], HEX);
  }
}
