package bp.algorithms.queues;

import java.util.Arrays;

/**
 * Created by vaksenov on 26.07.2019.
 */
public class Heap<K> {
    int size;
    PriorityNode<K>[] heap;
    PriorityNode<K> top;

    public Heap(int maxSize) {
        heap = new PriorityNode[maxSize + 1];
    }

    public void siftUp(PriorityNode<K> node) {
        int pos = node.pos;
        while (pos != 1) {
            if (node.compareTo(heap[pos / 2]) > 0) {
                return;
            }
            node.pos = pos / 2;
            heap[pos / 2].pos = pos;
            heap[pos] = heap[pos / 2];
            heap[pos / 2] = node;
            pos /= 2;
        }
    }

    public void siftDown(PriorityNode<K> node) {
        int pos = node.pos;
        while (2 * pos <= size) {
            PriorityNode<K> best = heap[2 * pos];
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
        }
    }

    public void insert(PriorityNode<K> node) {
        node.pos = ++size;
        if (size == heap.length) {
            heap = Arrays.copyOf(heap, 2 * heap.length);
        }
        heap[node.pos] = node;
        siftUp(node);
        top = heap[1].copy();
    }

    public PriorityNode<K> extractMin() {
        if (size == 1) {
            size--;
            top = null;
            return heap[1];
        }
        PriorityNode<K> res = heap[1];
        heap[1] = heap[size];
        size--;
        siftDown(heap[1]);
        top = heap[1].copy();
        return res;
    }

    public void changePriority(PriorityNode<K> node, double priority) {
        node.priority = priority;
        siftUp(node);
        siftDown(node);
        top = heap[1].copy();
    }

    public PriorityNode<K> peek() {
        return top;
    }

    public boolean check() {
        for (int i = 2; i <= size; i++) {
            if (heap[i / 2].compareTo(heap[i]) > 0) {
                return false;
            }
        }
        return true;
    }

    public String toString() {
        return Arrays.toString(heap);
    }
}
