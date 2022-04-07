package at.rocworks;

import gov.aps.jca.CAException;
import gov.aps.jca.Channel;
import gov.aps.jca.Context;

import java.text.SimpleDateFormat;
import java.util.Calendar;

import java.util.Date;
import java.util.HashMap;
import java.util.Timer;
import java.util.TimerTask;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.json.simple.JSONObject;

public class Meter {

    private MeterId meterId;
    private DataHdl dataHdl;
    
    private Date currPeriod = new Date(0);
    private Date prevPeriod = new Date(0);
    private Date prevSignal = new Date(0);
    
    private double prevSigs = 0;
    private double currSigs = 0;
    
    private Context epics = null;
    private HashMap<String, Channel> epicsTags = new HashMap<>();
    private HashMap<String, String> mqttTags = new HashMap<>();
    
    private MqttClient mqtt = null;
    
    private SimpleDateFormat fmt = new SimpleDateFormat("HH:mm:ss.SSS");

    private final int MQTT_QOS = 1;
        
    public Meter(MeterId meterId, DataHdl dataHdl, Context epics, MqttClient mqtt) {
        this.meterId = meterId;
        this.dataHdl = dataHdl;
        
        // Timer every 60 seconds
        Timer timer = new Timer();        
        Calendar c = Calendar.getInstance();
        
        c.add(Calendar.MINUTE, 1);
        c.set(Calendar.SECOND, 0);
        c.set(Calendar.MILLISECOND, 0);             
        
        timer.scheduleAtFixedRate(new TimerTask() {
            @Override
            public void run() {
                period(new Date());
            }
        }, c.getTime(), 60*1000);        
                
        // Epics
        if (epics!=null) {
            this.epics = epics;                        
            try {
                epicsTags.put("Signal", epics.createChannel(meterId.getName()+":Signal"));
                epicsTags.put("Value", epics.createChannel(meterId.getName()+":Value"));
                epicsTags.put("HighTime", epics.createChannel(meterId.getName()+":HighTime"));
                epicsTags.put("WattAct", epics.createChannel(meterId.getName()+":WattAct"));
                epics.flushIO();
            } catch (CAException | IllegalArgumentException | IllegalStateException ex) {
                Logger.getLogger(Meter.class.getName()).log(Level.SEVERE, null, ex);
            }            
        }
        
        if (mqtt!=null) {
            this.mqtt=mqtt;
            String name = meterId.getName();
            mqttTags.put("Id", name+"/Id");
            mqttTags.put("Signal", name+"/Signal");
            mqttTags.put("Value", name+"/Value");
            mqttTags.put("HighTime", name+"/HighTime");
            mqttTags.put("HighTimeNegative", name+"/HighTimeNegative");
            mqttTags.put("WattAct", name+"/WattAct");
            mqttTags.put("WattActJSON", name+"/WattActJSON");
        }        
    }

    private void dpSet(String dp, Integer value) {
        dpSetValue(dp, value);
    }
    private void dpSet(String dp, Long value)   { dpSetValue(dp, value); }
    private void dpSet(String dp, Double value) {
        dpSetValue(dp, value);
    }
    private void dpSet(String dp, String value) {
        dpSetValue(dp, value);
    }

    private void dpSetValue(String dp, Object value) {
        if (epics!=null) {
            try {
                Channel ch = epicsTags.get(dp);
                if (ch!=null) {
                    if (value instanceof Integer)
                        ch.put((Integer)value);
                    else if (value instanceof Double)
                        ch.put((Double)value);
                    else if (value instanceof String)
                        ch.put((String)value);
                    else if (value instanceof Long)
                        ch.put((Long)value);
                }
                epics.flushIO();
            } catch (CAException | IllegalStateException ex) {
                Logger.getLogger(Meter.class.getName()).log(Level.SEVERE, null, ex);
            }
        }

        String tag;
        if (mqtt != null && mqtt.isConnected() && (tag=mqttTags.get(dp))!=null) {
            JSONObject json = new JSONObject();
            json.put("Value", value);
            json.put("TimeMS", new Date().getTime());
            MqttMessage message = new MqttMessage(json.toJSONString().getBytes());
            message.setQos(MQTT_QOS);
            message.setRetained(true);

            try {
                mqtt.publish(tag, message);
            } catch (MqttException ex) {
                Logger.getLogger(Meter.class.getName()).log(Level.SEVERE, null, ex);
            }
        }
    }
    
    @Override
    public String toString() {
        return String.format("%s %s %s %s %3.3f %3.3f", meterId, 
                                fmt.format(prevPeriod), fmt.format(currPeriod), fmt.format(prevSignal),
                                prevSigs, currSigs);
    }
       
    volatile Date lastTimeHigh = new Date(0);
    Integer id = 0;
    
    public void signal(Date d, boolean high) {
        synchronized (this) {
            // Signal to Epics
            dpSet("Signal", high ? 1 : 0);
            //dpSet("Id", ++id);

            //Debug.println(meterId.getName() + " d=" + d.getTime() + " high=" + high + " lastTimeHigh=" + lastTimeHigh.getTime() + " diff=" + (d.getTime() - lastTimeHigh.getTime()));

            if (!high)
                return;

            // Watt Act
            if (lastTimeHigh.getTime() != 0) {
                Long time = d.getTime() - lastTimeHigh.getTime();  // ms
                if (time <= 0) {
                    Debug.println("### TIME <= 0! ### " + time + " ###");
                    dpSet("HighTimeNegative", time);
                    return;
                }
                Double watt = 3600.0 / time * 1000.0;  // 1000 Impulse/kWh
                dpSet("HighTime", time);
                dpSet("WattAct", watt);
            }
            lastTimeHigh = d;

            // Minute Watt Value
            currSigs++;
            if (prevSignal.getTime() > 0 && prevPeriod.getTime() > 0 && currSigs == 1) {

                double diff = d.getTime() - prevSignal.getTime();

                double add = 1.0 / diff * (currPeriod.getTime() - prevSignal.getTime());
                prevSigs += add;
                currSigs = (1 - add);
                //Debug.println("C " + meterId + " " + String.format("%3.3f %3.3f", diff, add));

                dpSet("Value", prevSigs);

                dataHdl.addRecord(new DataRecord(meterId, new Date(prevPeriod.getTime()), prevSigs));

                prevSigs = 0;
            }
            //Debug.println("S " + this + " " + (d.getTime()-prevSignal.getTime()));
            prevSignal.setTime(d.getTime());
        }
    }
    
    public synchronized void period(Date d) {
        /*
        Calendar c = Calendar.getInstance();        
        c.setTime(new Date());
        c.set(Calendar.MILLISECOND,0);
        */

        prevPeriod = currPeriod;
        prevSigs += currSigs;

        currPeriod = d;        
        currSigs = 0;      
        
        // Watt Act
        int n = 1; // 6;
        if (lastTimeHigh.getTime() < d.getTime()-n*60*1000 /* n Minutes, 10 Watt */ ) {
            dpSet("WattAct", 0.0);
        }      
        
        //Debug.println("P " + this);
    }

    public MeterId getMeterId() {
        return meterId;
    }        
}
