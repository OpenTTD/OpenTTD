/* $Id$ */

/**
 * Priority Queue.
 *  Peek and Pop always return the current lowest value in the list.
 *  Sort is done on insertion only.
 */
class PriorityQueue
{
	_queue = null;
	_count = 0;

	constructor()
	{
		_count = 0;
		_queue = [];
	}

	/**
	 * Insert a new entry in the list.
	 *  The complexity of this operation is O(n).
	 * @param item The item to add to the list.
	 * @param priority The priority this item has.
	 */
	function Insert(item, priority);

	/**
	 * Pop the first entry of the list.
	 *  This is always the item with the lowest priority.
	 *  The complexity of this operation is O(1).
	 * @return The item of the entry with the lowest priority.
	 */
	function Pop();

	/**
	 * Peek the first entry of the list.
	 *  This is always the item with the lowest priority.
	 *  The complexity of this operation is O(1).
	 * @return The item of the entry with the lowest priority.
	 */
	function Peek();

	/**
	 * Get the amount of current items in the list.
	 *  The complexity of this operation is O(1).
	 * @return The amount of items currently in the list.
	 */
	function Count();

	/**
	 * Check if an item exists in the list.
	 *  The complexity of this operation is O(n).
	 * @param item The item to check for.
	 * @return True if the item is already in the list.
	 */
	function Exists(item);
};

function PriorityQueue::Insert(item, priority)
{
	/* Append dummy entry */
	_queue.append(0);
	_count++;

	local i;
	/* Find the point of insertion */
	for (i = _count - 2; i >= 0; i--) {
		if (priority > _queue[i][1]) {
			/* All items bigger move one place to the right */
			_queue[i + 1] = _queue[i];
		} else if (item == _queue[i][0]) {
			/* Same item, ignore insertion */
			return false;
		} else {
			/* Found place to insert at */
			break;
		}
	}
	/* Insert new pair */
	_queue[i + 1] = [item, priority];

	return true;
}

function PriorityQueue::Pop()
{
	if (_count == 0) return null;

	local node = _queue.pop();
	_count--;

	return node[0];
}

function PriorityQueue::Peek()
{
	if (_count == 0) return null;

	return _queue[_count - 1][0];
}

function PriorityQueue::Count()
{
	return _count;
}

function PriorityQueue::Exists(item)
{
	/* Brute-force find the item (there is no faster way, as we don't have the priority number) */
	foreach (node in _queue) {
		if (node[0] == item) return true;
	}

	return false;
}
