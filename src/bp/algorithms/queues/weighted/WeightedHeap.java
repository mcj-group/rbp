package bp.algorithms.queues.weighted;

import java.util.Arrays;

/**
 * Created by vaksenov on 26.07.2019.
 */
public class WeightedHeap<K> {
    int size;
    WeightedPriorityNode<K>[] heap;
    WeightedPriorityNode<K> peek;

    public WeightedHeap(int maxSize) {
        heap = new WeightedPriorityNode[maxSize + 1];
        peek = new WeightedPriorityNode<>(null, -1, 0);
    }

    public double getWeight(int v) {
        if (v <= size) {
            return heap[v].maxWeight;
        } else {
            return 0;
        }
    }

    public void recalculateWeight(int v) {
        heap[v].maxWeight = Math.max(heap[v].weight, Math.max(getWeight(2 * v), getWeight(2 * v + 1)));
    }

    public void updateWeightsToRoot(int v) {
        while (v > 0) {
            recalculateWeight(v);
            v /= 2;
        }
    }

    public void siftUp(WeightedPriorityNode<K> node) {
        int pos = node.pos;
        while (pos != 1) {
            if (node.compareTo(heap[pos / 2]) > 0) {
                return;
            }
            node.pos = pos / 2;
            heap[pos / 2].pos = pos;
            heap[pos] = heap[pos / 2];
            heap[pos / 2] = node;
            recalculateWeight(pos);
            recalculateWeight(pos / 2);
            pos /= 2;
        }
        updateWeightsToRoot(pos);
    }

    public void siftDown(WeightedPriorityNode<K> node) {
        int pos = node.pos;

        updateWeightsToRoot(node.pos);

        while (2 * pos <= size) {
            WeightedPriorityNode<K> best = heap[2 * pos];
            if (2 * pos + 1 <= size && best.compareTo(heap[2 * pos + 1]) > 0) {
                best = heap[2 * pos + 1];
            }
            if (node.compareTo(best) < 0) {
                return;
            }
            node.pos = best.pos;
            best.pos = pos;
            pos = node.pos;
            heap[node.pos] = node;
            heap[best.pos] = best;
            recalculateWeight(node.pos);
            recalculateWeight(best.pos);
        }
    }

    public void insert(WeightedPriorityNode<K> node) {
        node.pos = ++size;
        if (size == heap.length) {
            heap = Arrays.copyOf(heap, 2 * heap.length);
        }
        heap[node.pos] = node;
        node.maxWeight = node.weight;
        siftUp(node);
        if (peek.priority == -1) {
            peek = heap[1].copy();
        } else {
            peek.copyFrom(heap[1]);
        }
    }

    public WeightedPriorityNode<K> extractMin() {
        if (size == 0) {
            return null;
        }
        if (size == 1) {
            size--;
            peek.priority = -1;
            heap[1] = null;
            return heap[1];
        }
        WeightedPriorityNode<K> res = heap[1];
        size--;
        updateWeightsToRoot((size + 1) / 2);
        heap[1] = heap[size + 1];
        heap[1].pos = 1;
        recalculateWeight(1);

        siftDown(heap[1]);
        peek.copyFrom(heap[1]);
        return res;
    }

    public void changePriority(WeightedPriorityNode<K> node, double priority) {
        node.priority = priority / node.totalUpdates;
        node.weight = priority;
        siftUp(node);
        siftDown(node);
        peek.copyFrom(heap[1]);
    }

    public WeightedPriorityNode<K> peek() {
        return peek.priority == -1 ? null : peek;
    }

    public WeightedPriorityNode<K> realPeek() {
        return heap[1];
    }

    public boolean check() {
        for (int i = size; i >= 1; i--) {
            double weightChildren = Math.max(
                    (2 * i <= size ? heap[2 * i].maxWeight : 0),
                    (2 * i + 1 <= size ? heap[2 * i + 1].maxWeight : 0));
            if (heap[i].maxWeight != Math.max(weightChildren, heap[i].weight)){
                return false;
            }
        }

        for (int i = 2; i <= size; i++) {
            if (heap[i / 2].compareTo(heap[i]) > 0) {
                return false;
            }
            if (heap[i / 2].maxWeight < heap[i].maxWeight) {
                return false;
            }
        }
        return true;
    }

    public String toString() {
        return Arrays.toString(heap);
    }
}
