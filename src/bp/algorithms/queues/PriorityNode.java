package bp.algorithms.queues;

/**
 * Created by vaksenov on 26.07.2019.
 */
public class PriorityNode<K> implements Comparable<PriorityNode<K>> {
    public int pos;
    public K value;
    public double priority;

    public PriorityNode(K value, double priority) {
        this.value = value;
        this.priority = priority;
    }

    public int compareTo(PriorityNode<K> node) {
        return Double.compare(node.priority, priority);
    }

    public PriorityNode<K> copy() {
        return new PriorityNode<>(value, priority);
    }

    public void copyFrom(PriorityNode<K> node) {
        this.value = node.value;
        this.priority = node.priority;
    }

    public String toString() {
        return  "{" + value + " " + priority + "}";
    }
}
