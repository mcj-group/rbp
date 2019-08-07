package bp.algorithms.queues.weighted;

import bp.algorithms.queues.PriorityNode;

/**
 * Created by vaksenov on 26.07.2019.
 */
public class WeightedPriorityNode<K> extends PriorityNode<K> implements Comparable<PriorityNode<K>> {
    public double weight;
    public double maxWeight;
    public int totalUpdates;

    public WeightedPriorityNode(K value, double priority, double weigth) {
        super(value, priority);
        this.weight = weigth;
        this.totalUpdates = 1;
    }

    public int compareTo(PriorityNode<K> node) {
        return Double.compare(node.priority, priority);
    }

    public WeightedPriorityNode<K> copy() {
        return new WeightedPriorityNode<K>(value, priority, weight);
    }

    public void copyFrom(WeightedPriorityNode<K> node) {
        super.copyFrom(node);
        this.weight = node.weight;
        this.maxWeight = node.maxWeight;
    }

    public String toString() {
        return  "{" + value + " " + priority + "}";
    }
}
