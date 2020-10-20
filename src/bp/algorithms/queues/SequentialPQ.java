package bp.algorithms.queues;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class SequentialPQ<K extends IdentifiedClass> implements PQ<K> {
    Heap<K> heap;
    PriorityNode<K>[] nodes;

    public SequentialPQ(int maxSize) {
        nodes = new PriorityNode[maxSize];
        heap = new Heap<>(maxSize);
    }

    public void insert(K value, double priority) {
        nodes[value.id] = new PriorityNode<>(value, priority);
        heap.insert(nodes[value.id]);
    }

    public void changePriority(K value, double newPriority) {
        heap.changePriority(nodes[value.id], newPriority);
    }

    public void changePriority(K value, double newPriority, double sensitivity) {
        if (Math.abs(nodes[value.id].priority - newPriority) < sensitivity)
            return;
        changePriority(value, newPriority);
    }

    public K extractMin() {
        return heap.extractMin().value;
    }

    public PriorityNode<K> extractNode() {
        return heap.extractMin();
    }

    public PriorityNode<K> peek() {
        return heap.peek();
    }

    public boolean check() {
        boolean[] have = new boolean[heap.heap.length + 1];
        for (int i = 1; i <= heap.size; i++) {
            int id = heap.heap[i].value.id;
            if (have[id]) {
                return false;
            }
            have[id] = true;
        }

        return heap.check();
    }

    public String toString() {
        return heap.toString();
    }
}
