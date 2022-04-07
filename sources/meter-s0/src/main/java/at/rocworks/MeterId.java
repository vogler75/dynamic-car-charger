package at.rocworks;

public class MeterId {

    private int custid, meterid;   
    private String name;
    
    public String toString() {
        return custid+"/"+meterid;
    }    
    
    public MeterId(int custid, int meterid, String name) {
        this.custid = custid;
        this.meterid = meterid;
        this.name = name;
    }

    public int getCustId() {
        return custid;
    }

    public int getMeterId() {
        return meterid;
    }
    
    public String getName() {
        return name;
    }
}
