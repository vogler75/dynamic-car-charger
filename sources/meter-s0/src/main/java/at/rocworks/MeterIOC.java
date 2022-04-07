package at.rocworks;/* Author: Andreas Vogler 2013 */

import com.pi4j.io.gpio.GpioController;
import com.pi4j.io.gpio.GpioFactory;
import com.pi4j.io.gpio.GpioPinDigitalInput;
import com.pi4j.io.gpio.PinPullResistance;
import com.pi4j.io.gpio.RaspiPin;
import com.pi4j.io.gpio.event.GpioPinDigitalStateChangeEvent;
import com.pi4j.io.gpio.event.GpioPinListenerDigital;

import gov.aps.jca.CAException;
import gov.aps.jca.Context;
import gov.aps.jca.JCALibrary;

import java.util.Date;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;

public class MeterIOC {
       
    public static void main(String[] args) throws ClassNotFoundException, InterruptedException, CAException {
        System.out.println("Start...");
        
        boolean epicsYes=false;
        String httpUrl=null;
        String mqttHost="localhost";
        for (int i=0; i<args.length; i++) {
            if (args[i].equals("-mqtt") && args.length>i+1) {
                mqttHost=args[i+1];
                System.out.println("-mqtt");
            }
            if (args[i].equals("-epics")) {
                epicsYes=true;
                System.out.println("-epics");
            }
            if (args[i].equals("-http") && args.length>i+1) {
                httpUrl=args[i+1];
                System.out.println("-http "+httpUrl);
            }
        }         
        
        //DataStoreHttp dsHttpRW = new DataStoreHttp("http://web:8080/apex/f?p=101:2:::NO::P2_CUSTID,P2_METERID,P2_TS,P2_VALUE:%d,%d,%s,%f");
        //DataStoreHttp dsHttpRW = new DataStoreHttp("https://web:8181/apex/apps/SaveMeterVal/%d/%d/%s/%f/1");
        //DataStoreHttp dsHttpOR = new DataStoreHttp("http://apex.oracle.com/pls/apex/f?p=1794:2:::NO::P2_ID,P2_TS,P2_VAL:%d,%d,%s,%f");        
        //DataStoreHttp dsHttpRW = new DataStoreHttp("http://web:8080/apex/apps/SaveMeterVal/%d/%d/%s/%f/1");
        DataStoreHttp dsHttpRW = new DataStoreHttp(httpUrl);
        DataStoreMySQL dsLocal = new DataStoreMySQL();
        
        DataHdl dataHdl = new DataHdl(dsHttpRW);
        dataHdl.setSecondaryStore(dsLocal, false);  
                               
        // create gpio controller
        System.out.println("Controller...");       
        GpioController gpio = GpioFactory.getInstance();
        System.out.println("Controller...done");       

        // provision gpio pin 
        //GpioPinDigitalOutput led1 = gpio.provisionDigitalOutputPin(RaspiPin.GPIO_00);

        // provision gpio pins as an input pin with its internal pull down resistor enabled
        System.out.println("Connect Pin 03..");       
        GpioPinDigitalInput pinInput = gpio.provisionDigitalInputPin(RaspiPin.GPIO_03, PinPullResistance.PULL_DOWN);
        
        System.out.println("Connect Pin 02...");       
        GpioPinDigitalInput pinOutput = gpio.provisionDigitalInputPin(RaspiPin.GPIO_02, PinPullResistance.PULL_DOWN);
        
        // Epics
        System.out.println("Epics...");       
        JCALibrary jca = JCALibrary.getInstance();
        final Context epicsContext = epicsYes ? jca.createContext("com.cosylab.epics.caj.CAJContext") : null;
                
        // MQTT
        System.out.println("Mqtt...");
        MqttClient mqttClient = null;           
        try {
            MqttConnectOptions options  = new MqttConnectOptions();     
            options.setAutomaticReconnect(true);
            mqttClient = new MqttClient("tcp://"+mqttHost, MeterIOC.class.getName());
            mqttClient.connect(options);
        } catch (MqttException ex) {
            Logger.getLogger(MeterIOC.class.getName()).log(Level.SEVERE, null, ex);
        }
        
        final Meter meterIn = new Meter(new MeterId(1, 1, "Meter_Input"), dataHdl, epicsContext, mqttClient); // input meter (power from provider)
        final Meter meterOut = new Meter(new MeterId(1, 2, "Meter_Output"), dataHdl, epicsContext, mqttClient); // output meter (power sent to provider)
                
        // create and register gpio pin listener
        System.out.println("Listen ..." + meterIn);
        final LinkedBlockingQueue<Message> inQueue = new LinkedBlockingQueue<Message>();

        pinInput.addListener(new GpioPinListenerDigital() {
                @Override
                public void handleGpioPinDigitalStateChangeEvent(GpioPinDigitalStateChangeEvent event) {   
                    //System.out.println("MeterIn "+event.getState().toString());
                    //meterIn.signal(new Date(), event.getState().isHigh());
                    try {
                        inQueue.put(new Message(event.getState().isHigh()));
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            });

        System.out.println("Listen ..." + meterOut);
        final LinkedBlockingQueue<Message> outQueue = new LinkedBlockingQueue<Message>();

        pinOutput.addListener(new GpioPinListenerDigital() {
                @Override
                public void handleGpioPinDigitalStateChangeEvent(GpioPinDigitalStateChangeEvent event) {
                    //System.out.println("MeterOut "+event.getState().toString());
                    //meterOut.signal(new Date(), event.getState().isHigh());
                    try {
                        outQueue.put(new Message(event.getState().isHigh()));
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            });
             
                     
        System.out.println(" ... PRESS <CTRL-C> TO STOP THE PROGRAM.");

        (new Thread() { public void run() {
            while (true) {
                Message m = null;
                try {
                    m = inQueue.take();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                if (m!=null)
                    meterIn.signal(m.time, m.high);
            }
        } }).start();

        (new Thread() { public void run() {
            while (true) {
                Message m = null;
                try {
                    m = outQueue.take();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                if (m!=null)
                    meterOut.signal(m.time, m.high);
            }
        } }).start();

        // keep program running until user aborts (CTRL-C)
        for (;;) {
            Thread.sleep(100);
        }
        
        // stop all GPIO activity/threads
        // (this method will forcefully shutdown all GPIO monitoring threads and scheduled tasks)
        // gpio.shutdown();   <--- implement this method call if you wish to terminate the Pi4J GPIO controller
    }

    public static class Message {
        Date time;
        boolean high;

        public Message(boolean high) {
            this.time=new Date();
            this.high=high;
        }

    }
}
