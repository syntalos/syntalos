
#pragma once

#include <QScopeGuard>

#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
#define syScopeGuard qScopeGuard
#else
template<typename F>
class SyScopeGuard
{
public:
    Q_NODISCARD_CTOR
    explicit SyScopeGuard(F &&f) noexcept
        : m_func(std::move(f))
    {
    }

    Q_NODISCARD_CTOR
    explicit SyScopeGuard(const F &f) noexcept
        : m_func(f)
    {
    }

    Q_NODISCARD_CTOR
    SyScopeGuard(SyScopeGuard &&other) noexcept
        : m_func(std::move(other.m_func)),
          m_invoke(std::exchange(other.m_invoke, false))
    {
    }

    ~SyScopeGuard() noexcept
    {
        if (m_invoke)
            m_func();
    }

    void dismiss() noexcept
    {
        m_invoke = false;
    }

    void commit() noexcept(std::is_nothrow_invocable_v<F>)
    {
        Q_ASSERT(m_invoke);
        m_invoke = false; // do it before we may throw from calling m_func()
        m_func();
    }

private:
    Q_DISABLE_COPY(SyScopeGuard)

    F m_func;
    bool m_invoke = true;
};

template<typename F>
SyScopeGuard(F (&)()) -> SyScopeGuard<F (*)()>;

template<typename F>
[[nodiscard]] SyScopeGuard<typename std::decay<F>::type> syScopeGuard(F &&f)
{
    return SyScopeGuard<typename std::decay<F>::type>(std::forward<F>(f));
}
#endif
