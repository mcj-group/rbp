package bp.algorithms.queues.weighted;

import bp.algorithms.queues.Heap;
import bp.algorithms.queues.IdentifiedClass;
import bp.algorithms.queues.PQ;
import bp.algorithms.queues.PriorityNode;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class WeightedSequentialPQ<K extends IdentifiedClass> {
    WeightedHeap<K> heap;
    WeightedPriorityNode<K>[] nodes;

    public WeightedSequentialPQ(int maxSize) {
        nodes = new WeightedPriorityNode[maxSize];
        heap = new WeightedHeap<>(maxSize);
    }

    public void insert(K value, double priority, double weight) {
        if (nodes[value.id] == null) {
            nodes[value.id] = new WeightedPriorityNode<>(value, priority, weight);
        }
        nodes[value.id].priority = priority;
        nodes[value.id].weight = weight;
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

    public WeightedPriorityNode<K> extractMin() {
        return heap.extractMin();
    }

    public WeightedPriorityNode<K> peek() {
        return heap.peek();
    }

    public boolean check() {
        return heap.check();
    }

    public String toString() {
        return heap.toString();
    }
}
