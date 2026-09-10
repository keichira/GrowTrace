#pragma once

#include "../Precompiled.h"
#include <functional>

/*template<typename... Args>
class CCallbackBase {
public:
    virtual ~CCallbackBase() {};
    virtual void Execute(Args... args) = 0;
};

template<class T, typename... Args>
class CCallback : public CCallbackBase<Args...> {
public:
    typedef void (T::*fn)(Args...);

public:
    CCallback(T& target, fn opr) : m_target(target), m_operation(opr) {}

public:
    void Execute(Args... args) override
    {
        (m_target.*m_operation)(args...);
    }

private:
    T& m_target;
    fn m_operation;
};*/

template <typename ReturnType = void, typename... Args> class Delegate
{
public:
    typedef ReturnType (*Opr)(void*, Args...);

public:
    Delegate() : m_target(nullptr), m_operation(nullptr) {}

public:
    template <class T, ReturnType (T::*fn)(Args...)> static Delegate Create(T* target)
    {
        Delegate delegate;
        delegate.m_target = target;
        delegate.m_operation = &FuncOpr<T, fn>;
        return delegate;
    }

    template <ReturnType (*fn)(Args...)> static Delegate Create()
    {
        Delegate delegate;
        delegate.m_target = nullptr;
        delegate.m_operation = &FuncOpr<fn>;
        return delegate;
    }

    ReturnType Execute(Args... args) const
    {
        if (m_operation)
        {
            return (*m_operation)(m_target, args...);
        }
        if constexpr (!std::is_void_v<ReturnType>)
        {
            return ReturnType();
        }
    }

    bool IsInitialized() const { return m_operation != nullptr; }

private:
    template <class T, ReturnType (T::*fn)(Args...)> static ReturnType FuncOpr(void* target, Args... args)
    {
        T* tg = (T*)target;
        return (tg->*fn)(args...);
    }

    template <ReturnType (*fn)(Args...)> static ReturnType FuncOpr(void*, Args... args) { return fn(args...); }

private:
    void* m_target;
    Opr m_operation;
};

template <typename EventType, typename ReturnType = void, typename... Args> class EventDispatcher
{
public:
    typedef Delegate<ReturnType, Args...> Handler;

public:
    EventDispatcher() {}

public:
    void Register(EventType type, Handler handler) { m_handlers[type].push_back(handler); }

    bool Dispatch(EventType type, Args... args)
    {
        auto it = m_handlers.find(type);
        if (it == m_handlers.end())
            return true;

        bool shouldForward = true;
        for (auto& handler : it->second)
        {
            if (handler.IsInitialized())
            {
                if constexpr (std::is_same_v<ReturnType, bool>)
                {
                    if (!handler.Execute(args...))
                    {
                        shouldForward = false;
                    }
                }
                else
                {
                    handler.Execute(args...);
                }
            }
        }
        return shouldForward;
    }

    bool HasHandler(EventType type) { return m_handlers.find(type) != m_handlers.end(); }

private:
    std::unordered_map<EventType, std::vector<Handler>> m_handlers;
};