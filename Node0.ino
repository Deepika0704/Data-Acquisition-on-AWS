#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <vmsock.h>
//Library to use DHT22 Sensor
#include <LDHT.h>
#include <LBattery.h>

//#include "mbedtls\net.h"
//#include <mtk.h>

#include <signal.h>
#include <limits.h>
/*#include "aws_iot_mqtt_interface.h"
#include "aws_iot_version.h"
#include "aws_iot_shadow_interface.h"
#include "aws_iot_shadow_json_data.h"
#include "aws_iot_json_utils.h"
#include "aws_iot_log.h"*/
#include "linkit_aws_header.h"
#include "aws_mtk_iot_config.h"
#ifdef connect
#undef connect
#endif
#include <LTask.h>
#include <LWiFi.h>
#include <LWiFiClient.h>
#include <LGPRS.h>

//Define Pin and type for DHT22
#define DHTPIN 2
#define DHTTYPE DHT22
//Define CO2 sensor for serial communication
#define sensor Serial1
//#define  TEMP_EN   1

void(* resetFunc) (void) = 0; 

const unsigned char cmd_get_sensor[] =
{
    0xff, 0x01, 0x86, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x79
};

unsigned char dataRevice[9];
//int temperature;
int CO2PPM = 0;

LDHT dht(DHTPIN, DHTTYPE);
float tempC = 0.0, Humi = 0.0;


float getTH(float *pTemp, float *pHumi, float *pCO2PPM){
  dht.begin();
  delay(1000);
  if (dht.read()){
      *pTemp = dht.readTemperature();
      *pHumi = dht.readHumidity();
      }
  if(dataRecieve())
    {
        *pCO2PPM = CO2PPM;
    }
}

/**
 * @brief Default MQTT HOST URL is pulled from the aws_iot_config.h
 */
char HostAddress[255] = AWS_IOT_MQTT_HOST;
/**
 * @brief Default MQTT port is pulled from the aws_iot_config.h
 */
VMINT port = AWS_IOT_MQTT_PORT;

char cafileName[] = AWS_IOT_ROOT_CA_FILENAME;
char clientCRTName[] = AWS_IOT_CERTIFICATE_FILENAME;
char clientKeyName[] = AWS_IOT_PRIVATE_KEY_FILENAME;

#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 200

QoSLevel qos = QOS_0;
int32_t i;
IoT_Error_t rc;

LWiFiClient c;
//define what you want in reported of shadow
typedef struct {
  double temperature;
  double humidity;
  int CO2_Level;
  //double heatIndex;
  //double dewPoint;
  bool windowOpen;
} ShadowReported;

ShadowReported reported;

char buff[256];

//define what you want in desired of shadow
typedef struct {
  bool windowOpen;
} ShadowDesired;

ShadowDesired desired;

char shadowTxBuffer[256];
char deltaBuffer[256];
boolean nativeLoop(void* user_data);

//Tells whether update was accepted or not!
void ShadowUpdateStatusCallback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
		const char *pReceivedJsonDocument, void *pContextData) {

//	if (pReceivedJsonDocument != NULL) {
//		DEBUG("Received JSON %s\n", pReceivedJsonDocument);
//	}
	if (status == SHADOW_ACK_TIMEOUT) {
		Serial.println("Update Timeout--");
	} else if (status == SHADOW_ACK_REJECTED) {
		Serial.println("Update RejectedXX");
	} else if (status == SHADOW_ACK_ACCEPTED) {
		Serial.println("Update Accepted !!");
	}
}

void windowActuate_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
	if (pContext != NULL) {
	    Serial.print("Delta - Window state changed to ");
            Serial.println(*(bool *)(pContext->pData));
	}
}

// initialize reported variables
MQTTClient_t mqttClient;
char *pJsonStringToUpdate;
float temperature = 0.0;
float humidity = 0.0;
float CO2_Level = 0.0;
char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
size_t sizeOfJsonDocumentBuffer;

bool windowOpen = false;
jsonStruct_t windowActuator;
jsonStruct_t temperatureHandler;
jsonStruct_t humidityHandler;
jsonStruct_t CO2_LevelHandler;
ShadowParameters_t sp;

// invoked in main thread context
void bearer_callback(VMINT handle, VMINT event, VMUINT data_account_id, void *user_data)
{
    if (VM_BEARER_WOULDBLOCK == g_bearer_hdl)
    {
        g_bearer_hdl = handle;
    }
    
    switch (event)
    {
        case VM_BEARER_DEACTIVATED:
            break;
        case VM_BEARER_ACTIVATING:
            break;
        case VM_BEARER_ACTIVATED:
              LTask.post_signal();
              break;
        case VM_BEARER_DEACTIVATING:
            break;
        default:
            break;
    }
}

boolean  mqtt_start(void* ctx)
{      
              rc = NONE_ERROR;
              i = 0;
	      aws_iot_mqtt_init(&mqttClient);

              sizeOfJsonDocumentBuffer = sizeof(JsonDocumentBuffer) / sizeof(JsonDocumentBuffer[0]);

              //do this for all reported variables
              windowActuator.cb = windowActuate_Callback;
              windowActuator.pData = &windowOpen;
              windowActuator.pKey = "windowOpen";
              windowActuator.type = SHADOW_JSON_BOOL;
              
              temperatureHandler.cb = NULL;
              temperatureHandler.pKey = "temperature";
              temperatureHandler.pData = &temperature;
              temperatureHandler.type = SHADOW_JSON_FLOAT;

              humidityHandler.cb = NULL;
              humidityHandler.pKey = "humidity";
              humidityHandler.pData = &humidity;
              humidityHandler.type = SHADOW_JSON_FLOAT;

              CO2_LevelHandler.cb = NULL;
              CO2_LevelHandler.pKey = "CO2_Level";
              CO2_LevelHandler.pData = &CO2_Level;
              CO2_LevelHandler.type = SHADOW_JSON_FLOAT;

              sp = ShadowParametersDefault;
	      sp.pMyThingName = AWS_IOT_MY_THING_NAME;
              sp.pMqttClientId = AWS_IOT_MQTT_CLIENT_ID;
	      sp.pHost = HostAddress;
	      sp.port = port;
	      sp.pClientCRT = AWS_IOT_CERTIFICATE_FILENAME;
	      sp.pClientKey = AWS_IOT_PRIVATE_KEY_FILENAME;
	      sp.pRootCA = AWS_IOT_ROOT_CA_FILENAME;
  
              Serial.print("  . Shadow Init... ");
              rc = aws_iot_shadow_init(&mqttClient);
              if (NONE_ERROR != rc) {
                Serial.println("Error in connecting...");
              }
              Serial.println("ok");
              
              rc = aws_iot_shadow_connect(&mqttClient, &sp);

	      if (NONE_ERROR != rc) {
		Serial.println("Shadow Connection Error");
	      }

              rc = aws_iot_shadow_register_delta(&mqttClient, &windowActuator);
              
              if (NONE_ERROR != rc) {
		Serial.println("Shadow Register Delta Error");
	      }

              //temperature = 0.0;
              //humidity = 0.0;
              
              Serial.println("  . mqtt_start finished...ok");
              
//              if (NONE_ERROR != rc) {
//		Serial.println("An error occurred in the loop.");
//	      }
//
//	      Serial.println("Disconnecting");
//	      rc = aws_iot_shadow_disconnect(&mqttClient);
//
//	      if (NONE_ERROR != rc) {
//		ERROR("Disconnect error");
//	      }
  
              return true;
}

boolean bearer_open(void* ctx){
    if (WIFI_USED)
        g_bearer_hdl = vm_bearer_open(VM_BEARER_DATA_ACCOUNT_TYPE_WLAN ,  NULL, bearer_callback);
    else
        g_bearer_hdl = vm_bearer_open(VM_APN_USER_DEFINE ,  NULL, bearer_callback);
    if(g_bearer_hdl >= 0)
        return true;
    return false;
}

/* Resolve IP address for AWS server */
VMINT wifiResolveCallback(vm_soc_dns_result *pDNS)
{
  //C_ADDRESS = (const char*)&pDNS->address[0];
  IN_ADDR addr;
  addr.S_un.s_addr = pDNS->address[0];
  CONNECT_IP_ADDRESS = inet_ntoa(addr);
//  Serial.println("wifiResolveCallback");
//  Serial.print("ip address is ");
//  Serial.println(CONNECT_IP_ADDRESS);
  LTask.post_signal();
  return 0;
}


boolean wifiResolveDomainName(void *userData)
{
  VMCHAR *domainName = (VMCHAR *)userData;
  vm_soc_dns_result dns;
  IN_ADDR addr;
  
//  Serial.print("in wifiResolveDomainName, host name is ");
//	Serial.println(domainName);

  VMINT resolveState;
  if (WIFI_USED){
        resolveState = vm_soc_get_host_by_name(VM_TCP_APN_WIFI,
                                 domainName,
                                 &dns,
                                 &wifiResolveCallback);
      Serial.flush();
  }
  else{
      Serial.flush();
        resolveState = vm_soc_get_host_by_name(6,
                                 domainName,
                                 &dns,
                                 &wifiResolveCallback);
      Serial.flush();
  }
                           
  if (resolveState > 0)
  {
    // not done yet
    return false;
  }

  switch (resolveState)
  {
  case VM_E_SOC_SUCCESS:  // Get IP address successfully, result is filled.
    addr.S_un.s_addr = dns.address[0];
    CONNECT_IP_ADDRESS = inet_ntoa(addr);
    Serial.print("ip address is ");
    Serial.println(CONNECT_IP_ADDRESS);

    return true;
  case VM_E_SOC_WOULDBLOCK:  // wait response from network, result could be gotten from callback.
    // need to wait, return directly
    // so MMI message loop may continue.
    return false;
  case VM_E_SOC_INVAL:  // invalid arguments: null domain_name, etc.
  case VM_E_SOC_ERROR:  // unspecified error
  case VM_E_SOC_LIMIT_RESOURCE:  // socket resources not available
  case VM_E_SOC_INVALID_ACCOUNT:  // invalid data account id
    return true;
  }
}

/* Main setup function */
void setup()
{
  LTask.begin();

  sensor.begin(9600);
  Serial.begin(9600);
  while(!Serial)
    delay(100);

  // keep retrying until connected to AP
  if (WIFI_USED){
    LWiFi.begin();
    Serial.print("  . Connecting to AP...");
    Serial.flush();
    while (0 == LWiFi.connect(WIFI_AP, LWiFiLoginInfo(WIFI_AUTH, WIFI_PASSWORD)))
    {
      delay(1000);
    }
  }
  else{  
    Serial.print("  . Connecting to GPRS...");
    Serial.flush();
    while (!LGPRS.attachGPRS(GPRS_APN, GPRS_USERNAME, GPRS_PASSWORD))
    {
      delay(500);
    }
  }

  Serial.println("ok");
  dht.begin();
  
  LTask.remoteCall(&wifiResolveDomainName, (void*)HostAddress);
 
//  CONNECT_IP_ADDRESS = IP_ADDRESS;
  CONNECT_PORT = port;
  
  LTask.remoteCall(&bearer_open, NULL);
  LTask.remoteCall(&mqtt_start, NULL);
}

/* for analogRead or other API, some may not be able to called in remoteCall. 
You may need to call it in loop function and then pass the parameter to nativeLoop through a pointer. */
void loop()
{
    int aa[1];
    int bb[1];
    int gg[1];

    sprintf(buff,"battery level = %d", LBattery.level() );
  Serial.println(buff);
  sprintf(buff,"is charging = %d",LBattery.isCharging() );
  Serial.println(buff);

    if(dataRecieve())
    {
        //Serial.print("Temperature: ");
        //Serial.print(temperature);
        //Serial.print("  CO2: ");
        //Serial.print(CO2PPM);
        //Serial.println("");
        gg[0] = CO2PPM;
    }
    //delay(1000);
    aa[0] = dht.readTemperature();
    bb[0] = dht.readHumidity();
    Serial.flush();
    updateTH();
LTask.remoteCall(nativeLoop, (void*)aa);
LTask.remoteCall(nativeLoop, (void*)bb);
LTask.remoteCall(nativeLoop, (void*)gg);
}


/* message could be passed value to publish_Shadow */
int publish_Shadow(char * topic, char * message) {
    int rc = NONE_ERROR;
    
    rc = aws_iot_shadow_yield(&mqttClient, 3000);   //please don't try to put it lower than 1000, otherwise it may going to timeout easily and no response  
    delay(1000);
    //static unsigned long timer_t1 = millis();
    // update per 2000ms
    //if(millis()-timer_t1 > 1000)
    
    Serial.println("=======================================================================================");
    Serial.print("On Device: window state ");
    if (windowOpen)
        Serial.println("true");
    else
        Serial.println("false");
        
    if (temperature > 20)
         windowOpen = true;
    rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
    if (rc == NONE_ERROR) {
      //All reported variables to be printed
        rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 4, &CO2_LevelHandler, &temperatureHandler, &humidityHandler, &windowActuator); 
        if (rc == NONE_ERROR) {
	    rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
            if (rc == NONE_ERROR){
	        Serial.print("Update Shadow: ");
                Serial.println(JsonDocumentBuffer);
		rc = aws_iot_shadow_update(&mqttClient, AWS_IOT_MY_THING_NAME, JsonDocumentBuffer, ShadowUpdateStatusCallback, NULL, 4, true);
                Serial.print(" rc for aws_iot_shadow_update is ");
                Serial.println(rc);
            }
        }
     }
     Serial.println("*****************************************************************************************");
     return rc;
}

char mqtt_message[2048];
boolean nativeLoop(void* user_data) {
    
    int *bb = (int*)user_data;
    sprintf(mqtt_message, "%s : your passing message", *bb);
    
    publish_Shadow("mtkTopic5", mqtt_message);
    Serial.flush();
}

/* if enabled, update temperature value from sensor */
void updateTH()
{
    static unsigned long timer_t = millis();
    
    // update per 2000ms
    if(millis()-timer_t > 2000)
    {
        timer_t = millis();
        getTH(&temperature, &humidity, &CO2_Level);
        Serial.print("update temp: ");
        Serial.println(temperature);
        Serial.print("update humi: ");
        Serial.println(humidity);
        Serial.print("update co2: ");
        Serial.println(CO2_Level);
    }
}

bool dataRecieve(void)
{
    byte data[9];
    int i = 0;

    //transmit command data
    for(i=0; i<sizeof(cmd_get_sensor); i++)
    {
        sensor.write(cmd_get_sensor[i]);
    }
    delay(10);
    //begin reveiceing data
    if(sensor.available())
    {
        while(sensor.available())
        {
            for(int i=0;i<9; i++)
            {
                data[i] = sensor.read();
            }
        }
    }

    for(int j=0; j<9; j++)
    {
        Serial.print(data[j]);
        Serial.print(" ");
    }
    Serial.println("");

    if((i != 9) || (1 + (0xFF ^ (byte)(data[1] + data[2] + data[3] + data[4] + data[5] + data[6] + data[7]))) != data[8])
    {
        return false;
    }

    CO2PPM = (int)data[2] * 256 + (int)data[3];
    //temperature = (int)data[4] - 40;

    return true;
}

