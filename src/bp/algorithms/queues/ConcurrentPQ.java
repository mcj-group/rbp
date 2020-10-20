package bp.algorithms.queues;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class ConcurrentPQ<K extends IdentifiedClass> extends SequentialPQ<K> {

    public ConcurrentPQ(int maxSize) {
        super(maxSize);
    }

    public synchronized void insert(K value, double priority) {
        super.insert(value, priority);
    }

    public synchronized void changePriority(K value, double newPriority) {
        super.changePriority(value, newPriority);
    }

    public void changePriority(K value, double newPriority, double sensitivity) {
        if (Math.abs(nodes[value.id].priority - newPriority) < sensitivity)
            return;
        changePriority(value, newPriority);
    }

    public synchronized K extractMin() {
        return super.extractMin();
    }

    public synchronized PriorityNode<K> extractNode() {
        return super.extractNode();
    }

    public PriorityNode<K> peek() {
        return heap.peek();
    }

    public synchronized boolean check() {
        return super.check();
    }

    public synchronized String toString() {
        return heap.toString();
    }
}
