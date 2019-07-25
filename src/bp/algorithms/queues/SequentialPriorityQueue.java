package bp.algorithms.queues;

import java.util.Arrays;
import javafx.util.Pair;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class SequentialPriorityQueue<K extends IdentifiedClass> {

    public class Node<K extends IdentifiedClass> implements Comparable<Node> {
        int pos;
        K value;
        double priority;

        public Node(int pos, K value, double priority) {
            this.pos = pos;
            this.value = value;
            this.priority = priority;
        }

        public int compareTo(Node node) {
            return Double.compare(node.priority, priority);
        }

        public String toString() {
            return "{" + value + " " + priority + "}";
        }
    }

    Node<K>[] nodes;
    Node<K>[] heap;
    int size;

    public SequentialPriorityQueue(int maxSize) {
        heap = new Node[maxSize + 1];
        nodes = new Node[maxSize];
    }

    public void siftUp(Node<K> node) {
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

    public void siftDown(Node<K> node) {
        int pos = node.pos;
        while (2 * pos < size) {
            Node<K> best = heap[2 * pos];
            if (2 * pos + 1 < size && best.compareTo(heap[2 * pos + 1]) > 0) {
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

    public void insert(K value, double priority) {
        nodes[value.id] = new Node<>(++size, value, priority);
        heap[nodes[value.id].pos] = nodes[value.id];
        siftUp(nodes[value.id]);
    }

    public void changePriority(K value, double newPriority) {
        nodes[value.id].priority = newPriority;
        siftUp(nodes[value.id]);
        siftDown(nodes[value.id]);
    }

    public Pair<K, Double> peek() {
        return new Pair<>(heap[1].value, heap[1].priority);
    }

    public String toString() {
        return Arrays.toString(heap);
    }
}
