package at.rocworks;

import java.sql.ResultSet;

public abstract class DataStore {
    Object saveRecordOK = new Object();
    
    public abstract DataRecord.State saveRecord(DataRecord rec);
    public abstract DataRecord.State saveRecord(DataRecord rec, DataRecord.State state);
    
    public abstract ResultSet findRecords(DataRecord.State state);
    public abstract DataRecord nextRecord(ResultSet rs);    
}
