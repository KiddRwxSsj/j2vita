package javax.microedition.m3g;

import java.util.Vector;

public class Group extends Node {
    final Vector children = new Vector();

    public Group() {
    }

    public void addChild(Node c) {
        if (c == null) throw new NullPointerException();
        if (c == this || c instanceof World) throw new IllegalArgumentException();
        if (c.parent != null && c.parent != this) throw new IllegalArgumentException();
        if (!children.contains(c)) {
            children.addElement(c);
            c.parent = this;
            version++;
        }
    }

    public void removeChild(Node c) {
        if (c != null && children.removeElement(c)) {
            c.parent = null;
            version++;
        }
    }

    public int getChildCount() { return children.size(); }

    public Node getChild(int i) {
        if (i < 0 || i >= children.size()) throw new IndexOutOfBoundsException();
        return (Node)children.elementAt(i);
    }

    void refs(Vector v) {
        super.refs(v);
        for (int i = 0; i < children.size(); i++) v.addElement(children.elementAt(i));
    }
}
