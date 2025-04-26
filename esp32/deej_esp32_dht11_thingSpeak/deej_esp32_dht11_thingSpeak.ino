//#define VERBOSE_DHT11

const int NUM_SLIDERS = 6;	//14;//5;
const int analogInputs[NUM_SLIDERS] = {36, 39, 34, 35, 32, 33};	//{4, 15, 13, 12, 14, 27, 26, 25, 33, 32, 35, 34, 39, 36};//{A0, A1, A2, A3, A4};
const int NUM_BUTTONS = 9;	//10;
const int buttonInputs[NUM_BUTTONS] = {16,  4, 23, 15,  25, 26, 27, 14, 12}; //, 13};

//!!!!!  b3 - switch com defeito !!!!!

//-------------------------
//  b1  |  |  |  |  |  |  b6
//  b2  |  |  |  |  |  |  b7
//  b3 s1 s2 s3 s4 s5 s6  b8
//  b4  |  |  |  |  |  |  b9
//  b5  |  |  |  |  |  |  MIC
//-------------------------

#define MIC_BTN	13	// btn mic
#define MIC_LED	 2	// Led da placa

float 	pf[]{75, 75, 75, 75, 75, 75}, 		//polo do filtro
		dT = 10e-3;		//tempo de amostragem (delay de 10 ms)
int analogSliderValues[NUM_SLIDERS];

double analogFiltrado[NUM_SLIDERS];

int SliderValues[NUM_SLIDERS];
bool buttonValues[NUM_BUTTONS];

bool mic = false;
bool micDebounce = false;

String LastSentData = "";

unsigned long micMillis;
unsigned long micInterval = 500;
void IRAM_ATTR IntMic(){
  if(micMillis + micInterval < millis()){
    micMillis = millis();
    //micDebounce = true;
    mic = !mic;
    digitalWrite(MIC_LED, mic);
  }
}

TaskHandle_t TaskDeej;

void loopDeej(void * pvParameters){
  for(;;){
    for (int i = 0; i < NUM_SLIDERS; i++)
      analogSliderValues[i] = analogRead(analogInputs[i]);

    for (int i = 0; i < NUM_BUTTONS; i++)
      buttonValues[i] = digitalRead(buttonInputs[i]);
    
    //a cada interrupção do btnMic troca o estado do mic
    //if(micDebounce){
    //  mic = !mic;
    //  digitalWrite(MIC_LED, mic);
    //  micDebounce = false;
    //}

    for (int i = 0; i < NUM_SLIDERS; i++) {
      analogFiltrado[i] += dT*pf[i]*( ((double)analogSliderValues[i]) - analogFiltrado[i]);	//filtro do sinal: xf(k) = xf(k-1) + W*(x(k)-xf(k))
      
      SliderValues[i] = ((double)10.23*floor( (analogFiltrado[i]/40.95) + 0.5 ));	//arredonda
    }

    sendSliderValues(); // Actually send data (all the time)

    vTaskDelay(10);
  }
}

void sendSliderValues() {
	String builtString = String("");

	builtString += "s";
	for (int i = 0; i < NUM_SLIDERS; i++) {
		builtString += String((int)SliderValues[i]);
		builtString += String("|s");
	}
	
	builtString += mic?"1023":"0";
	
	for (int i = 0; i < NUM_BUTTONS; i++) {
		builtString += String("|b");
		builtString += buttonValues[i]?"1":"0";
	}
	
	if(builtString != LastSentData){
		Serial.println(builtString);		
		LastSentData = builtString;
	}
}

////////////////////////////DHT11////////////////////////////
#include <WiFi.h>
#include "secrets.h"  //define SECRET_SSID, SECRET_PASS, SECRET_CH_ID, SECRET_WRITE_APIKEY
#include "ThingSpeak.h"

const char *ssid = SECRET_SSID;
const char *pass = SECRET_PASS;
int keyIndex = 0;            // your network key Index number (needed only for WEP)
WiFiClient  client;

unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;

#define   DHT_PIN   22
#include "DHT.h"
DHT dht11(DHT_PIN, DHT11);
float humi = 0, temp = 0;
uint64_t lastRead = 0;

TaskHandle_t TaskDht11;

void loopDht11(void * pvParameters){
  for(;;){
    // Connect or reconnect to WiFi
    if(WiFi.status() != WL_CONNECTED){
      //Serial.print("Attempting to connect to SSID: ");
      //Serial.println(SECRET_SSID);
      while(WiFi.status() != WL_CONNECTED){
        WiFi.begin(ssid, pass);  // Connect to WPA/WPA2 network. Change this line if using open or WEP network
        #ifdef VERBOSE_DHT11
          Serial.print(".");
        #endif
        vTaskDelay(5000);     
      }
      #ifdef VERBOSE_DHT11
        Serial.println("\nConnected.");
      #endif
    }

    uint64_t t0 = esp_timer_get_time();
    if(t0 > lastRead + 60000000){  //delay de 2s entre cada leitura
      humi = dht11.readHumidity();
      temp = dht11.readTemperature();
      lastRead = t0;

      if ( isnan(temp) || isnan(humi)) {
        #ifdef VERBOSE_DHT11
          Serial.println("Failed to read from DHT11 sensor!");
        #endif
      }else{
        #ifdef VERBOSE_DHT11
          Serial.println("temp = "+String(temp)+" °C");
          Serial.println("humi = "+String(humi)+" %");
        #endif

        // set the fields with the values
        ThingSpeak.setField(1, temp);
        ThingSpeak.setField(2, humi);
        // write to the ThingSpeak channel
        int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
        #ifdef VERBOSE_DHT11
          if(x == 200){
            Serial.println("Channel update successful.");
          }
          else{
            Serial.println("Problem updating channel. HTTP error code " + String(x));
          }
        #endif
      }
    }
    vTaskDelay(1);
  }
}
//////////////////////////!!DHT11!!//////////////////////////

void setup() { 
  Serial.begin(115200);
  while(!Serial);

	for (int i = 0; i < NUM_SLIDERS; i++)
		pinMode(analogInputs[i], INPUT);

	for (int i = 0; i < NUM_BUTTONS; i++)
		pinMode(buttonInputs[i], INPUT_PULLUP);
	
	pinMode(MIC_LED, OUTPUT);
	digitalWrite(MIC_LED, mic);
	
	pinMode(MIC_BTN, INPUT_PULLUP);
	attachInterrupt (MIC_BTN, IntMic, FALLING);

	dht11.begin(); // initialize the DHT11 sensor
  WiFi.mode(WIFI_STA);   
  ThingSpeak.begin(client);  // Initialize ThingSpeak

    //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
                    loopDht11,   /* Task function. */
                    "TaskDht11", /* name of task. */
                    10240,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &TaskDht11,  /* Task handle to keep track of created task */
                    0);          /* pin task to core 1 */
  
  //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
                    loopDeej,   /* Task function. */
                    "TaskDeej", /* name of task. */
                    10240,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &TaskDeej,  /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
}

void loop(){vTaskSuspend(NULL);}
