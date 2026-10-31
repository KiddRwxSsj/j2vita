package javax.microedition.sensor;

public final class Unit {
    private final String symbol;

    private Unit(String symbol) {
        this.symbol = symbol;
    }

    public static Unit getUnit(String symbol) {
        if (symbol == null) throw new NullPointerException();
        return new Unit(symbol);
    }

    public String toString() {
        return symbol;
    }
}
