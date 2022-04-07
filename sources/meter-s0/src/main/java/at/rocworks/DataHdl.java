package at.rocworks;
/* Author: Andreas Vogler 2013 */

import java.sql.ResultSet;

import java.sql.SQLException;
import java.sql.Statement;
import java.util.ArrayList;

public class DataHdl {
    
    private enum ThreadType {
        WRITE,
        CHECK        
    }
    
    private enum State {
        UNKNOWN,
        ALIVE,
        BROKEN
    }    
    
    private ArrayList<DataRecord> records = new ArrayList<DataRecord>();    
    private DataStore dsPrimary, dsSecondary;
    private State sPrimaryState = State.UNKNOWN; // -1 unknown, 0 false, 1 true
    private boolean bSecondaryBadOnly = false;
    private int iBadRecords = -1;
    
    private int countDown = 3;
    private int counter = 0;
    
    private int checkTimerMin = 10; // Minutes
            
    public DataHdl(DataStore dsPrimary) {
        this.dsPrimary = dsPrimary;
        this.dsSecondary = null;
        
        class Task implements Runnable {
            private ThreadType t;
            public Task(ThreadType t) { this.t = t; }
            public void run() {
                switch ( t ) {
                case WRITE : writeThread(); break;
                case CHECK : checkThread(); break;
                default: break;
                }
            }
        }                
        
        (new Thread(new Task(ThreadType.WRITE))).start();
        (new Thread(new Task(ThreadType.CHECK))).start();
    }

    
    public void setSecondaryStore(DataStore dsSecondary, boolean bSecondaryBadOnly) {
        this.dsSecondary = dsSecondary;
        this.bSecondaryBadOnly = bSecondaryBadOnly;
    }    
    

        
    public void addRecord(DataRecord rec) {
        synchronized ( records ) {
            //Debug.println("addRecord "+rec.toSQL());
            records.add(rec);
            if ( counter == 0 ) 
                counter=countDown;
        }
    }
    
    private void writeThread() {
        ArrayList<DataRecord> recordsCopy = new ArrayList<DataRecord>();    
        
        while ( true )
        try {            
            Thread.sleep(1000);       
            
            synchronized ( records ) {                
                if ( counter > 0 ) {
                    counter--;
                    if ( counter == 0 ) {                        
                        for ( DataRecord rec : records ) 
                            recordsCopy.add(rec);
                        records.clear();
                    }
                }
            }
            
            if ( ! recordsCopy.isEmpty() ) {                
                //Debug.println("writeThread..." + recordsCopy.size());
                sPrimaryState = State.UNKNOWN;
                for ( DataRecord rec : recordsCopy ) 
                    storeRecord(rec);
                recordsCopy.clear();
                               
                if ( sPrimaryState == State.ALIVE && (iBadRecords > 0 || iBadRecords == -1) )
                    retryBadRecords();
                //Debug.println("writeThread...done");
            }            
        } catch ( Exception e ) {
            Debug.println(e.toString());
        }
    }
    
    private void checkThread() {
        while ( true )
        try {            
            Thread.sleep(1000*60*checkTimerMin);       
            retryBadRecords();            
        } catch ( InterruptedException e ) {
            e.printStackTrace();
        }        
    }
       
    private void storeRecord(DataRecord rec) {
        //Debug.println("storeRecord "+rec);
        
        DataRecord.State ret;
        
        if ( sPrimaryState != State.BROKEN ) {  
            ret = dsPrimary.saveRecord(rec);
            sPrimaryState = ( ret==DataRecord.State.OK ? State.ALIVE : State.BROKEN );
        } else {
            ret = DataRecord.State.BAD;
        }
        
        if ( (dsSecondary != null) && (!bSecondaryBadOnly || ret==DataRecord.State.BAD) ) {                     
            if ( dsSecondary != null ) 
                dsSecondary.saveRecord(rec, ret);
            if ( ret==DataRecord.State.BAD ) 
                iBadRecords++;                        
        }        
    }
    
    private void retryBadRecords() {
        if ( dsSecondary == null ) return;
        synchronized ( this ) {
            //Debug.println("retryBadRecords...");
            DataRecord rec;                
            DataRecord.State ret;
            iBadRecords=0;
            ResultSet rs = dsSecondary.findRecords(DataRecord.State.BAD);
            if ( rs != null ) {
                while ( ((rec=dsSecondary.nextRecord(rs)) != null) && (++iBadRecords>0) &&
                        ((ret=dsPrimary.saveRecord(rec)) == DataRecord.State.OK) ) {                        
                    dsSecondary.saveRecord(rec, DataRecord.State.OK);
                    iBadRecords--;
                }
            }
            try {
                Statement stmt = rs.getStatement();
                rs.close();
                stmt.close();
            } catch (SQLException e) {
                e.printStackTrace();
            }
            //Debug.println("retryBadRecords...done");
        }
    }
}
