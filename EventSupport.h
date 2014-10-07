
#include <set>
// Для std::pair
#include <utility>
// Для HWND и DWORD
#include <windef.h>
// Для PostMessage
#include <winuser.h>

class EventSubscriber {
    /// Окно, которое должно получать указанные события.
    HWND hWnd;
    /// Битовая маска активированных для получения событий:
    ///   SERVICE_EVENTS
    ///   USER_EVENTS
    ///   SYSTEM_EVENTS
    ///   EXECUTE_EVENTS
    DWORD mask;
public:
    /*enum Event {
        Service = SERVICE_EVENTS,
        User    = USER_EVENTS,
        System  = SYSTEM_EVENTS,
        Execute = EXECUTE_EVENTS,
    };
    typedef Flags<Event> EventFlags;*/
public:
    EventSubscriber(HWND hWnd, DWORD mask) hWnd(hWnd), mask(mask) {}
    bool operator==(const EventSubscriber& other) const {
        return hWnd == other.hWnd;
    }
    bool operator<(const EventSubscriber& other) const {
        return hWnd < other.hWnd;
    }
public:
    /// Добавляет к маске новое отслеживаемое событие.
    void add(DWORD event) {
        mask |= event;
    }
    /// Удаляет из маски указанный класс отслеживаемых событий.
    /// @return `true`, если после удаления события больше нет подписки ни на одно из событий, иначе `false`.
    bool remove(DWORD event) {
        mask &= ~event;
        return mask == 0;
    }
    void notify(DWORD event) const {
        if (mask & event) {
            PostMessage(hWnd, (DWORD)event, 0, 0);
        }
    }
};
class EventNotifier {
    typedef std::set<EventSubscriber> SubscriberList;
    typedef std::pair<SubscriberList::iterator, bool> InsertResult;
private:
    SubscriberList subscribers;
public:
    void add(HWND hWnd, DWORD event) {
        InsertResult r = subscribers.insert(EventSubscriber(hWnd, event));
        // Если указанный элемент уже существовал в карте, то он не будет заменен,
        // нам это и не нужно. Вместо этого нужно обновить существующий элемент.
        if (r.second) {
            r.first->add(event);
        }
    }
    void remove(HWND hWnd, DWORD event) {
        SubscriberList::iterator it = subscribers.find(EventSubscriber(hWnd, event));
        if (it != subscribers.end()) {
            // Удаляем класс событий. Если это был последний интересуемый класс событий,
            // то удаляем и подписчика.
            if (it->remove(event)) {
                subscribers.erase(it);
            }
        }
    }
    /// Уведомляет всех подписчиков об указанном событии.
    void notify(DWORD event) const {
        for (typename SubscriberList::const_iterator it = subscribers.begin(); subscribers.end(); ++it) {
            it->second.notify(event);
        }
    }
};
