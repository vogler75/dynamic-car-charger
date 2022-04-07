package at.rocworks;

import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.json.simple.JSONObject;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.text.DateFormat;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.logging.Level;
import java.util.logging.Logger;

public class SMA {

    public static void main(String[] args) {
        System.out.println("Start..."+SMA.class.getName());
        for (int i=0; i<args.length; i++) {
            if (args[i].equals("-mqtt") && args.length>i+1) {
                mqttHost=args[i+1];
                System.out.println("-mqtt");
            }
        }
        new SMA().start();
    }

    private final String SMADATA = "/proj/smadata/data/MyPlant-Spot-LastValues.csv";

    private final String SMASPOT = "/proj/smadata/get_spot";
    private final String SMAMINS = "/proj/smadata/get_mins";
    private final String SMADAYS = "/proj/smadata/get_days";

    public static String mqttHost = "localhost";
    private MqttClient mqtt = null;
    private final int MQTT_QOS = 1;

    public void start() {
        System.out.println("Mqtt...");
        try {
            MqttConnectOptions options  = new MqttConnectOptions();
            options.setAutomaticReconnect(true);
            mqtt = new MqttClient("tcp://"+mqttHost, SMA.class.getName());
            mqtt.connect(options);
        } catch (MqttException ex) {
            Logger.getLogger(this.getClass().getName()).log(Level.SEVERE, null, ex);
        }

        System.out.println("Threads...");
        (new Thread() { public void run() { spotData(); } }).start();
        (new Thread() { public void run() { minsData(); } }).start();
        (new Thread() { public void run() { daysData(); } }).start();

    }

    private void spotData() {
        while (true)
            try {
                Calendar cal = Calendar.getInstance();
                cal.setTime(new Date());
                int h = cal.get(Calendar.HOUR_OF_DAY);
                if ( h >= 6 && h <= 22 ) {
                    Date t1=new Date();
                    synchronized (this) {
                        //System.out.println("SpotData...");
                        system(SMASPOT);
                        Thread.sleep(1000);
                        //System.out.println("SpotData...done");
                    }
                    importSpot();
                    //long w = 60000 - ((new Date().getTime())-t1.getTime());
                    //System.out.println("SpotData...import done, wait "+w);
                    //if (w>0) Thread.sleep(w);
                } else {
                    Thread.sleep(1000 * 60);
                }
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
    }

    private void minsData() {
        while (true)
        try {
            Calendar cal = Calendar.getInstance();
            cal.setTime(new Date());
            int h = cal.get(Calendar.HOUR_OF_DAY);
            if ( h >= 6 && h <= 22 ) {
                synchronized (this) {
                    //System.out.println("MinsData...");
                    system(SMAMINS);
                    Thread.sleep(1000);
                    //System.out.println("MinsData...done");
                }
                Thread.sleep(1000 * 60 * 5);
            } else {
                Thread.sleep(1000 * 60);
            }

        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private void daysData() {
        boolean done = false;
        while (true)
        try {
            Calendar cal = Calendar.getInstance();
            cal.setTime(new Date());
            int h = cal.get(Calendar.HOUR_OF_DAY);
            if (h == 4) {
                if (!done) {
                    synchronized (this) {
                        //System.out.println("DaysData...");
                        system(SMADAYS);
                        Thread.sleep(1000);
                        //System.out.println("DaysData...done");
                    }
                    done=true;
                }
            } else {
                done=false;
            }
            Thread.sleep(1000 * 60);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private String system(String cmd) {
        Runtime r = Runtime.getRuntime();
        try {
            Process p = r.exec(cmd);
            p.waitFor();
            StringBuilder s = new StringBuilder();
            BufferedReader b = new BufferedReader(new InputStreamReader(p.getInputStream()));
            String line;
            while ((line = b.readLine()) != null) {
                s.append(line);
            }
            b.close();
            return s.toString();
        } catch (IOException e) {
            e.printStackTrace();
            return e.toString();
        } catch (InterruptedException e) {
            e.printStackTrace();
            return e.toString();
        }
    }

    private Date importSpotLastTS = new Date();
    private void importSpot() {
        try {
            List<String> lines = Files.readAllLines(Paths.get(SMADATA), StandardCharsets.ISO_8859_1);
            for (String line : lines) {
                String[] cols = line.split(";");
                DateFormat format = new SimpleDateFormat("dd/MM/yyyy HH:mm:ss", Locale.ENGLISH);
                try {
                    Date ts = format.parse(cols[0]);
                    if (ts.getTime() > importSpotLastTS.getTime()) {
                        List<String> dps = Arrays.asList(
                                "PV/Spot/PDC1_WATT",
                                "PV/Spot/PDC2_WATT",
                                "PV/Spot/IDC1_AMP",
                                "PV/Spot/IDC2_AMP",
                                "PV/Spot/UDC1_VOLT",
                                "PV/Spot/UDC2_VOLT",
                                "PV/Spot/PAC1_WATT",
                                "PV/Spot/PAC2_WATT",
                                "PV/Spot/PAC3_WATT",
                                "PV/Spot/IAC1_AMP",
                                "PV/Spot/IAC2_AMP",
                                "PV/Spot/IAC3_AMP",
                                "PV/Spot/UAC1_VOLT",
                                "PV/Spot/UAC2_VOLT",
                                "PV/Spot/UAC3_VOLT",
                                "PV/Spot/PDCTOT_WATT",
                                "PV/Spot/PACTOT_WATT",
                                "PV/Spot/EFFICIENCY_PRC",
                                "PV/Spot/ETODAY_KWH",
                                "PV/Spot/ETOTAL_KWH",
                                "PV/Spot/FREQUENCEY_HZ",
                                "PV/Spot/OPERATINGTIME_HOURS",
                                "PV/Spot/FEEDINTIME_HOURS",
                                "PV/Spot/BT_SIGNAL_PRC"
                        );
                        for (int i=0; i<dps.size(); i++) {
                            double val = Double.parseDouble(cols[i+1]);
                            dpSetTimed(dps.get(i), ts, val);
                        }
                        importSpotLastTS=ts;
                    }
                } catch (ParseException e) {
                    e.printStackTrace();
                }

            };
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void dpSet(String tag, Object value) {
        dpSetTimed(tag, new Date(), value);
    }

    private void dpSetTimed(String tag, Date ts, Object value) {
        if (mqtt != null && mqtt.isConnected()) {
            JSONObject json = new JSONObject();
            json.put("Value", value);
            json.put("TimeMS", ts.getTime());
            MqttMessage message = new MqttMessage(json.toJSONString().getBytes());
            message.setQos(MQTT_QOS);
            message.setRetained(true);

            try {
                mqtt.publish(tag, message);
            } catch (MqttException ex) {
                ex.printStackTrace();
            }
        }
    }
}
