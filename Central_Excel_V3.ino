
/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/*
 * Copyright Johannes Peter Schramel Vetmeduni Vienna 2018, 2019
 * Central UART to interface to excel  PLX-DAQ 2.11 for Excel by Net^Devil  
 */

#include <bluefruit.h>

#define autoSave     (20)            // in minutes (20)
#define LED          (17)            // red LED on feather nrf52
//#define TEMP                         // show temp in column (small animal applications)

BLEClientBas  clientBas;  // battery client
BLEClientDis  clientDis;  // device information client
BLEClientUart clientUart; // bleuart client

bool fin = false;
bool pause = false;
uint8_t ch = 0;
           
unsigned long  checkbox_timer = millis();           // timer to query checkbox status
unsigned long  autoSave_timer = millis(); 

boolean tare_cmd = false;
boolean height_cmd = false;

void setup()
{
  Serial.begin(115200);
  //while ( !Serial ) delay(10);   // for nrf52840 with native usb

  Serial.println("Central Excel GW");
  Serial.println("-----------------------------------\n");
  
  digitalWrite(LED,LOW);                // initial state of LED = off  
  pinMode(LED, OUTPUT); 
  
  // Initialize Bluefruit with maximum connections as Peripheral = 0, Central = 1
  // SRAM usage required by SoftDevice will increase dramatically with number of connections
  Bluefruit.configCentralBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin(0, 1);
  Bluefruit.setTxPower(4);                //Values are: -40, -30, -20, -16, -12, -8, -4, 0, 3, 4 (5,6,7,8, nrf52840 only) 
  Bluefruit.configCentralBandwidth(BANDWIDTH_MAX);
  Bluefruit.setName("Excel GW");

  // Configure Battery client
  clientBas.begin();  

  // Configure DIS client
  clientDis.begin();

  // Init BLE Central Uart Serivce
  clientUart.begin();
  clientUart.setRxCallback(bleuart_rx_callback);

  // Increase Blink rate to different from PrPh advertising mode
  Bluefruit.setConnLedInterval(250);

  // Callbacks for Central
  Bluefruit.Central.setConnectCallback(connect_callback);
  Bluefruit.Central.setDisconnectCallback(disconnect_callback);

  /* Start Central Scanning
   * - Enable auto scan if disconnected
   * - Interval = 100 ms, window = 80 ms
   * - Don't use active scan
   * - Start(timeout) with timeout = 0 will scan forever (until connected)
   */
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  //Bluefruit.Scanner.filterRssi(-92);              // has effect only on reconnect field strength                   
  Bluefruit.Scanner.setInterval(160, 80);           // in unit of 0.625 ms
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.start(0);                       // 0 = Don't stop scanning after n seconds

//setup excel sheet
  Serial.println("CLEARSHEET");
  //Serial.println("CLEARDATA");
  #ifdef TEMP
  Serial.println("LABEL,date,time,yaw,pitch,roll,dphi,temp,stat");
  #endif
  #ifndef TEMP
  Serial.println("LABEL,date,time,yaw,pitch,roll,dphi,alt,stat");
  #endif
  Serial.println("CUSTOMBOX1,LABEL,TARE ALT");  // set name of checkbox 
  Serial.println("CUSTOMBOX2,LABEL, ");         // clear name
  Serial.println("CUSTOMBOX3,LABEL, ");         // clear name
  Serial.println("CUSTOMBOX1,SET,0");           // preset checkbox
  Serial.println("DONE");                       // flush buffer 
}

/**
 * Callback invoked when scanner pick up an advertising data
 * @param report Structural advertising data
 */
void scan_callback(ble_gap_evt_adv_report_t* report)
{
  // Check if advertising contain BleUart service
  if ( Bluefruit.Scanner.checkReportForService(report, clientUart) )
  {
    Serial.print("BLE UART service detected. Connecting ... ");

    // Connect to device with bleuart service in advertising
    Bluefruit.Central.connect(report);
  }else
  {      
    // For Softdevice v6: after received a report, scanner will be paused
    // We need to call Scanner resume() to continue scanning
    Bluefruit.Scanner.resume();
  }
}

/**
 * Callback invoked when an connection is established
 * @param conn_handle
 */
void connect_callback(uint16_t conn_handle)
{
  Serial.println("Connected");

  Serial.print("Dicovering Device Information ... ");
  if ( clientDis.discover(conn_handle) )
  {
    Serial.println("Found it");
    char buffer[32+1];
    
    // read and print out Manufacturer
    memset(buffer, 0, sizeof(buffer));
    if ( clientDis.getManufacturer(buffer, sizeof(buffer)) )
    {
      Serial.print("Manufacturer: ");
      Serial.println(buffer);
    }

    // read and print out Model Number
    memset(buffer, 0, sizeof(buffer));
    if ( clientDis.getModel(buffer, sizeof(buffer)) )
    {
      Serial.print("Model: ");
      Serial.println(buffer);
    }

    Serial.println();
  }else
  {
    Serial.println("Found NONE");
  }

  Serial.print("Dicovering Battery ... ");
  if ( clientBas.discover(conn_handle) )
  {
    Serial.println("Found it");
    Serial.print("Battery level: ");
    Serial.print(clientBas.read());
    Serial.println("%");
  }else
  {
    Serial.println("Found NONE");
  }

  Serial.print("Discovering BLE Uart Service ... ");
  if ( clientUart.discover(conn_handle) )
  {
    Serial.println("Found it");

    Serial.println("Enable TXD's notify");
    clientUart.enableTXD();

    Serial.println("Ready to receive from peripheral");
  }else
  {
    Serial.println("Found NONE");
    
    // disconnect since we couldn't find bleuart service
    Bluefruit.disconnect(conn_handle);
  }  
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 * https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/cores/nRF5/nordic/softdevice/s140_nrf52_6.1.1_API/include/ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
  
  Serial.println("Disconnected");
  fin = false;                                // to start  with a new line   
}

/**
 * Callback invoked when uart received data
 * @param uart_svc Reference object to the service where the data 
 * arrived. In this example it is clientUart
 */
void bleuart_rx_callback(BLEClientUart& uart_svc)
{
 
  // Forward from BLEUART to HW Serial
  if (fin == false && pause == false)
  {
    //Serial.println("BEEP");               // indicate received data with a beep
    Serial.print("DATA,DATE,TIME,");      // write just one time
    fin = true;
    delay(50);                            // Wait to fill buffer check duration with nrf logger/toolbox
  }
    if ( pause == true ) uart_svc.flush();  // do not accumulate data during pause
    while ( uart_svc.available() ) 
      {
      ch = (uint8_t)uart_svc.read();
      if (ch == '\n') 
        {
        Serial.println(",AUTOSCROLL_-10");  // negative values let end the row in the middle
        //Serial.print("\r\n");             // adds CR for excel software
        fin = false;                        // line is finished     
        }
      else Serial.write(ch);
      }
  
}

void loop()
{
  if( millis() - autoSave_timer > autoSave * 60000 && fin == false)    // automatic storage of excel to avoid data loss
  {
  pause = true;                                 // do not accept data during pause
  autoSave_timer = millis();
   
  Serial.println("DONE");                       // flush buffer 
  Serial.println("PAUSELOGGING");
  delay(1000);                                  // delays are empirical excel may hang/ does not get resume command
  Serial.println("SAVEWORKBOOK");
  delay(500);
  Serial.println("RESUMELOGGING");
  
  pause = false;                                // accept data again
  }
  

  
  if( millis() - checkbox_timer > 1000 && fin == false)  // only allowed if all data has been transferred is ready
  {
    checkbox_timer  = millis();
    Serial.println ("CUSTOMBOX1,GET");   // Checkbox query
    tare_cmd = Serial.readStringUntil(10).toInt();
    if(tare_cmd == true) clientUart.print("t");
    tare_cmd = false;
    /*
    Serial.println ("CUSTOMBOX2,GET");   // Checkbox query
    height_cmd = Serial.readStringUntil(10).toInt();
    if(height_cmd == true) clientUart.print("h");
    height_cmd = false;
    */
  }
  
  if ( Bluefruit.Central.connected() )
  {
    // Not discovered yet
    if ( clientUart.discovered() )
    {
      // Discovered means in working state
      // Get Serial input and send to Peripheral
      if ( Serial.available() )
      {
        delay(2); // delay a bit for all characters to arrive
        
        char str[20+1] = { 0 };
        Serial.readBytes(str, 20);
        
        clientUart.print( str );
      }
    }
  }

}
