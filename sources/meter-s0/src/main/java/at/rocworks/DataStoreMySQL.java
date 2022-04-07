package at.rocworks;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;

import java.text.SimpleDateFormat;

public class DataStoreMySQL extends DataStore  {
    
    private SimpleDateFormat fmt = new SimpleDateFormat("yyyyMMddHHmmss");      

    public DataStoreMySQL() throws ClassNotFoundException {
        Class.forName(Driver);    
        Open();
    }
    
    public DataStoreMySQL(String Driver, String Username, String Password, String URL) throws ClassNotFoundException {
        Class.forName(Driver);    
        this.Driver = Driver;
        this.Username = Username;
        this.Password = Password;
        this.URL = URL;
        Open();
    }    
    
    protected void finalize() throws Throwable {
        Close();
    }
    
    private String Driver = "com.mysql.jdbc.Driver";       
    private String Username = "root";
    private String Password = "root";
    private String URL = "jdbc:mysql://localhost:3306/rocv";    
    private Connection connection;       

    @Override
    public DataRecord.State saveRecord(DataRecord rec) {     
        return saveRecord(rec, DataRecord.State.NULL);
    }
    
    @Override
    public DataRecord.State saveRecord(DataRecord rec, DataRecord.State state) {       
        int i;
        DataRecord.State ret; 
        
        if ( !isConnected()) {
            Close();
            Open();
        }
        
        String s = String.format(
            "INSERT INTO MeterVal (custid, meterid, ts, value, state) " +
            "VALUES (%d, %d, '%s', %f, %d) " +
            "ON DUPLICATE KEY UPDATE value = %f, state=%d"
            , 
            rec.getMeterId().getCustId(), 
            rec.getMeterId().getMeterId(), 
            fmt.format(rec.getTs()), 
            rec.getVal(), state.getCode(),
            rec.getVal(), state.getCode());
        
        //Debug.println("saveRecord "+s);
        
        ret=(Execute(s) ? DataRecord.State.OK : DataRecord.State.BAD);
        
        synchronized ( saveRecordOK ) {
            if ( ret==DataRecord.State.OK )
                saveRecordOK.notify();
        }
        
        return ret;
    }    
    
    @Override
    public ResultSet findRecords(DataRecord.State state) {
        return Query("SELECT custid, meterid, ts, value " +
                     "FROM MeterVal " + 
                     "WHERE state="+state.getCode());        
    }
    
    @Override
    public DataRecord nextRecord(ResultSet rs) {
        try {
            if ( rs.next() ) {
                return new DataRecord(new MeterId(rs.getInt("custid"), rs.getInt("meterid"), ""),
                                      rs.getTimestamp("ts"), 
                                      rs.getFloat("value"));
            }
            else
                return null;
        } catch ( SQLException e ) {
            e.printStackTrace();
            return null;
        }
    }    
        

    private boolean Open() {
        try {
            connection = DriverManager.getConnection(this.URL, this.Username, this.Password);
            return connection.isValid(0);
        } catch (Exception e) {
            e.printStackTrace();
            Debug.println("Error Connecting with User:" + Username + " and Password:" + Password);
            return false;
        }
    }  
    
    private void Close() {
        if (connection != null) {
            try {
                connection.close();
            } catch (Exception e) {
            }
        }
    }
    
    private boolean isConnected() {
        try {
            return connection.isValid(0);
            /*
            ResultSet rs = this.Query("SELECT 1;");
            if (rs == null) {
                return false;
            }
            if (rs.next()) {
                return true;
            }
            return false;
            */
        } catch (Exception e) {
            return false;
        }
    }
    
    private ResultSet Query(String query) {
        try {
            //Debug.println("Query: "+query);
            Statement stmt = this.connection.createStatement();
            ResultSet rs = stmt.executeQuery(query);
            return rs;
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }
    
    private boolean Execute(String query) {
        try {
            Statement stmt = this.connection.createStatement();
            boolean ret = stmt.execute(query);
            stmt.close();
            return ret;
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
    }    
}
