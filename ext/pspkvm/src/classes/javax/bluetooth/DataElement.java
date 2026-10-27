package javax.bluetooth;

import java.util.Enumeration;
import java.util.Vector;

public class DataElement {
    public static final int NULL = 0x0000;
    public static final int U_INT_1 = 0x0008;
    public static final int U_INT_2 = 0x0009;
    public static final int U_INT_4 = 0x000A;
    public static final int U_INT_8 = 0x000B;
    public static final int U_INT_16 = 0x000C;
    public static final int INT_1 = 0x0010;
    public static final int INT_2 = 0x0011;
    public static final int INT_4 = 0x0012;
    public static final int INT_8 = 0x0013;
    public static final int INT_16 = 0x0014;
    public static final int URL = 0x0040;
    public static final int UUID = 0x0018;
    public static final int BOOL = 0x0028;
    public static final int STRING = 0x0020;
    public static final int DATSEQ = 0x0030;
    public static final int DATALT = 0x0038;

    private final int type;
    private long num;
    private boolean flag;
    private Object value;

    public DataElement(int valueType) {
        type = valueType;
        if (type == DATSEQ || type == DATALT) value = new Vector();
    }
    public DataElement(boolean bool) { type = BOOL; flag = bool; }
    public DataElement(int valueType, long value) { type = valueType; num = value; }
    public DataElement(int valueType, Object value) { type = valueType; this.value = value; }

    public void addElement(DataElement elem) { ((Vector)value).addElement(elem); }
    public void insertElementAt(DataElement elem, int index) { ((Vector)value).insertElementAt(elem, index); }
    public int getSize() { return ((Vector)value).size(); }
    public boolean removeElement(DataElement elem) { return ((Vector)value).removeElement(elem); }
    public int getDataType() { return type; }
    public long getLong() { return num; }
    public boolean getBoolean() { return flag; }
    public Object getValue() {
        return value instanceof Vector ? ((Vector)value).elements() : value;
    }
}
