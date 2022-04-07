package at.rocworks;

import java.text.SimpleDateFormat;

import java.util.Date;

public class DataRecord {
    
    public enum State {   
        NULL(0),
        OK(1),
        BAD(-1);
        
        private int code;
        
        private State(int c) {
          code = c;
        }
        
        public int getCode() {
          return code;
        }        
    }    
    
    private SimpleDateFormat fmt = new SimpleDateFormat("yyyy.MM.dd HH:mm:ss.SSS");                                     
    
    private MeterId meterId;
    
    private Date ts;
    private double val;
    
    public DataRecord(MeterId meterId, Date ts, double val) {
       this.meterId = meterId; 
       this.ts = ts;
       this.val = val;
    }

    public MeterId getMeterId() {
        return meterId;
    }

    public Date getTs() {
        return ts;
    }

    public double getVal() {
        return val;
    }
    
    public String toString() {
        return meterId + " " + fmt.format(ts);
    }
    
    public String toSQL() {
        String s = String.format(
            "INSERT INTO MeterVal (custid, meterid, ts, value, state) " +
            "VALUES (%d, %d, '%s', %f, %d) " 
            , 
            meterId.getCustId(), 
            meterId.getMeterId(), 
            fmt.format(this.ts), 
            this.val, -1);
        
        return s;
    }
}
