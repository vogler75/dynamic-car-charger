package at.rocworks;

import java.io.BufferedReader;

import java.io.IOException;
import java.io.InputStreamReader;

import java.net.URL;

import java.text.SimpleDateFormat;
import java.net.*;

import java.sql.ResultSet;


import org.json.simple.parser.JSONParser;
import org.json.simple.JSONObject;
import org.json.simple.parser.ParseException;

public class DataStoreHttp extends DataStore {
    private SimpleDateFormat fmt = new SimpleDateFormat("yyyyMMddHHmmss");      
    private String url;
    
    public DataStoreHttp(String url) {
        this.url = url;
    }
    
    @Override
    public DataRecord.State saveRecord(DataRecord rec) {    
        return saveRecord(rec, DataRecord.State.OK);
    }

    @Override
    @SuppressWarnings("oracle.jdeveloper.java.nested-assignment")
    public DataRecord.State saveRecord(DataRecord rec, DataRecord.State state) {
        if (url==null) return DataRecord.State.OK;
        DataRecord.State ret;
        try {
            String s = String.format(url, 
                                     rec.getMeterId().getCustId(), 
                                     rec.getMeterId().getMeterId(), 
                                     fmt.format(rec.getTs()), 
                                     rec.getVal());
            
            URL u = new URL(s);            
            //Debug.println("saveRecord "+s);
            URLConnection conn = u.openConnection();
            BufferedReader in = new BufferedReader(
                                    new InputStreamReader(
                                    conn.getInputStream()));
            int retcode;
            String inputLine;
        
            while ((inputLine = in.readLine()) != null) 
            {
                //System.out.println(inputLine);
                try {
                    JSONObject json = (JSONObject)new JSONParser().parse(inputLine);
                    retcode = Integer.parseInt(json.get("retcode").toString());
                    ret = (retcode >= 0 ? DataRecord.State.OK : DataRecord.State.BAD);
                        
                } catch ( ParseException e)  {
                    System.out.println(e);
                    ret = DataRecord.State.BAD;
                }                
            }
            in.close();
           
            ret = DataRecord.State.OK;
        } catch ( java.net.MalformedURLException e ) {
            e.printStackTrace();
            ret = DataRecord.State.BAD;
        } catch ( IOException e ) {
            e.printStackTrace();            
            ret = DataRecord.State.BAD;
        }        
        
        synchronized ( saveRecordOK ) {
            if ( ret==DataRecord.State.OK )
                saveRecordOK.notify();
        }
        
        return ret;
    }    
    
    @Override
    public ResultSet findRecords(DataRecord.State state) {
        assert true : "not implemented!";
        return null;
    }
    
    @Override
    public DataRecord nextRecord(ResultSet rs) {
        assert true : "not implemented!";
        return null;
    }
    
}
