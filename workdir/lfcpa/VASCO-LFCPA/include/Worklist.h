
#ifndef COPYCONSTANTPROPAGATION_WORKLIST_H
#define COPYCONSTANTPROPAGATION_WORKLIST_H

template <typename Value, typename KeyHash = std::hash<Value>>
class Worklist {
private:
    std::stack<Value> mStack;
    std::unordered_set<Value, KeyHash> mSet;

public:
    Worklist() : mStack(), mSet() {}

    bool empty() const {
        return mStack.empty();
    }

    size_t size() const {
        return mStack.size();
    }

    bool workInsert(Value val) {
        bool status = mSet.insert(val).second;
        if (status) {
            // `val` was not present in the set. Hence, it was
            // inserted in the set as well as the stack.
            mStack.push(std::move(val));
        }
        return status;
    }

    /* NOTE: It is assumed that the user will not call this method when stack is
     * empty */
    Value workDelete() {
        Value val = std::move(mStack.top());
        mStack.pop();
        mSet.erase(val);
        return val;
    }
};

#endif // COPYCONSTANTPROPAGATION_WORKLIST_H
