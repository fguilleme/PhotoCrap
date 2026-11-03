#pragma once
#include <QHash>
#include <QMutex>
#include <array>
#include <functional>
#include <memory>

namespace PhotoCrap::Render
{
// Shared accounting of application-owned canvas textures, not driver/Qt UI VRAM.
// Visible resources are pinned; the budget is an eviction target, not a hard cap.
class GraphicsBudget
{
public:
    enum Category { Photo, Mask, Effects, Stack, Other, CategoryCount };
    struct Usage {
        qint64                            resident = 0, pinned = 0, target = 0, peak = 0;
        std::array<qint64, CategoryCount> categories{};
    };
    class Client
    {
    public:
        ~Client();
        void   invalidateWith(std::function<void()> callback);
        void   configure(qint64 target);
        void   pin(qint64 bytes);
        void   report(Category category, qint64 bytes);
        qint64 allowance() const;
        qint64 available() const;

    private:
        friend class GraphicsBudget;
        Client(std::shared_ptr<GraphicsBudget> budget, quint64 id)
            : m_budget(std::move(budget)), m_id(id)
        {
        }
        void                            invalidateOthers();
        std::shared_ptr<GraphicsBudget> m_budget;
        quint64                         m_id;
    };
    static std::shared_ptr<GraphicsBudget> shared();
    static std::shared_ptr<Client>         attach(const std::shared_ptr<GraphicsBudget> &budget,
                                                  qint64                                 target);
    Usage                                  usage() const;

private:
    struct Entry {
        qint64                            target = 0, pinned = 0;
        std::array<qint64, CategoryCount> bytes{};
        std::function<void()>             invalidate;
    };
    mutable QMutex        m_mutex;
    QHash<quint64, Entry> m_entries;
    quint64               m_next = 0;
    qint64                m_peak = 0;
    Usage                 usageLocked() const;
};
} // namespace PhotoCrap::Render
