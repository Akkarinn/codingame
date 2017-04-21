
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

// REPERE (0-1600,0-9000)
//     x
//    -----> 
//    |
// y  |
//    |
//    v

// MAP STATICS
static const int DELIVERY_RADIUS    = 1600;
static const int STUN_RADIUS        = 1760;
static const int BUST_MIN_RADIUS    = 900;
static const int BUST_MAX_RADIUS    = 1760;
static const int MAP_WIDTH          = 16001;
static const int MAP_HEIGHT         = 9001;

// KEYPOINT
struct Point {
    explicit Point(int x = 0, int y = 0, int id = -1) : x(x), y(y), id(id) {}
    int x;
    int y;
    int id;
};

static const Point g_home0 = Point(0, 0);
static const Point g_home1 = Point(16000, 9000);
static const Point g_camp_home0 = Point( 1100, 1100);
static const Point g_camp_home1 = Point(14900, 7900);

static const std::size_t NB_KEYPOINTS = 16;

//     0   1       2       3
//  7      6       5         4
//  8      9      10        11
//   15   14      13      12
static const Point g_keypoints[NB_KEYPOINTS] = {
    Point( 1100, 1100, 0),
    Point( 5500, 1100, 1),
    Point(10000, 1100, 2),
    Point(14900, 1100, 3),
    //
    Point(14900, 4000, 4),
    Point(10000, 4000, 5),
    Point( 5500, 4000, 6),
    Point( 1100, 4000, 7),
    
    //
    Point( 1100, 6500, 8),
    Point( 5500, 6500, 9),
    Point(10000, 6500, 10),
    Point(14900, 6500, 11),
    //
    Point(14900, 8900, 12),
    Point(10000, 8900, 13),
    Point( 5500, 8900, 14),
    Point( 1100, 8900, 15),
    
};

struct Buster {
    struct State {
        enum Type {
            Empty,
            Carry,
            Stunned,
            Busting
        };
    };
    Buster(const Buster& o)
        : id(o.id)
        , x(o.x)
        , y(o.y)
        , state(o.state)
    {
    }
    Buster(int id, int x, int y, State::Type state)
        : id(id)
        , x(x)
        , y(y)
        , state(state)
    {
    }
    bool operator<(const Buster& o) const { return id < o.id; }
    int id;
    int x;
    int y;
    State::Type state;
};

struct Ghost {
    Ghost& operator=(const Ghost& o)
    {
        if (this != &o) {
            id = o.id;
            x = o.x;
            y = o.y;
        }
        
        return *this;
    }
    Ghost(int id, int x, int y)
        : id(id)
        , x(x)
        , y(y)
    {
    }
    bool operator<(const Ghost& o) const { return id < o.id; }
    int id;
    int x;
    int y;
};

struct KnowledgeBase {
    int m_myTeamId;
    int m_bustersPerPlayer;
    int m_ghostCount;
    
    std::unordered_map<int, Ghost>  m_ghosts;
    std::unordered_map<int, Buster> m_busters0;
    std::unordered_map<int, Buster> m_busters1;
    
    std::set<Ghost>   m_currentGhosts;
    std::set<Buster>  m_currentEnnemies;

    KnowledgeBase()
    {
        // the amount of busters you control
        cin >> m_bustersPerPlayer; cin.ignore();
        // the amount of ghosts on the map
        cin >> m_ghostCount; cin.ignore();
        // if this is 0, your base is on the top left of the map, if it is one, on the bottom right
        cin >> m_myTeamId; cin.ignore();
    }
    void step() {
        std::cerr << "[kb] ===============" << std::endl; 
        m_currentGhosts.clear();
        m_currentEnnemies.clear();
        int entities; // the number of busters and ghosts visible to you
        cin >> entities; cin.ignore();
        for (int i = 0; i < entities; i++) {
            int entityId; // buster id or ghost id
            int x;
            int y; // position of this buster / ghost
            int entityType; // the team id if it is a buster, -1 if it is a ghost.
            int state; // For busters: 0=idle, 1=carrying a ghost.
            int value; // For busters: Ghost id being carried. For ghosts: number of busters attempting to trap this ghost.
            cin >> entityId >> x >> y >> entityType >> state >> value; cin.ignore();
            updateEntity(entityId, x, y, entityType, state, value);
        }
    }
private:
    void updateEntity(int entityId, int x, int y, int entityType, int state, int value) {
        if (entityType == -1) {
             auto it = m_ghosts.find(entityId);
             if (it == m_ghosts.end())
                m_ghosts.insert(std::make_pair(entityId, Ghost(entityId, x, y)));
            else
                it->second = Ghost(entityId, x, y);
            m_currentGhosts.insert(Ghost(entityId, x, y));
            std::cerr << "[kb] see Ghost #" << entityId << " " << x << " " << y << std::endl;
        }
        else if (entityType == m_myTeamId) {
             auto it = m_busters0.find(entityId);
             if (it == m_busters0.end())
                m_busters0.insert(std::make_pair(entityId, Buster(entityId, x, y, static_cast<Buster::State::Type>(state))));
            else
                it->second = Buster(entityId, x, y, static_cast<Buster::State::Type>(state));
            std::cerr << "[kb] my Buster Ally #" << entityId << " " << x << " " << y << " " << state << std::endl;
        }
        else {
             auto it = m_busters1.find(entityId);
             if (it == m_busters1.end())
                m_busters1.insert(std::make_pair(entityId, Buster(entityId, x, y, static_cast<Buster::State::Type>(state))));
            else
                it->second = Buster(entityId, x, y, static_cast<Buster::State::Type>(state));
            m_currentEnnemies.insert(Buster(entityId, x, y, static_cast<Buster::State::Type>(state)));
            std::cerr << "[kb] see Buster Ennemy #" << entityId << " " << x << " " << y << " " << state << std::endl;
        }
    }
};

struct ActionProcessor {
    static void move(int x, int y) { std::cout << "MOVE " << x  << " " << y << " on the move "/* << x << " " << y */<< std::endl; }
    static void bust(int id) { std::cout << "BUST " << id << " BUST U!" << std::endl; }
    static void stun(int id) { std::cout << "STUN " << id << " STUN U!" << std::endl; }
    static void release() { std::cout << "RELEASE" << " Releasing!" << std::endl; }
};

struct NavigationEngine {
    const KnowledgeBase& m_kb;
    explicit NavigationEngine(const KnowledgeBase& kb) : m_kb(kb) {}
    
    // Attack
    static bool isInStunableRadius(double distance) {
        return distance < STUN_RADIUS;
    }
    // Capture
    static bool isInBustableRadius(double distance) {
        return distance >= BUST_MIN_RADIUS && distance < BUST_MAX_RADIUS;
    }
    static bool isBustable(int busterX, int busterY, int ghostX, int ghostY) {
        return isInBustableRadius(distance(Point(busterX, busterY), Point(ghostX, ghostY)));
    };
    bool isInLineOfSight(std::size_t busterId, const Point& target)  const {
        return false;
    }
    bool hasReachTarget(const Buster& buster, const Point& target) const {
        return (0 == std::abs(buster.x - target.x) &&
                0 == std::abs(buster.y - target.y));    
    }
    static double distance(const Point& a, const Point& b) {
        const int h = std::abs(a.x - b.x);
        const int w = std::abs(a.y - b.y);
        return std::sqrt(h * h + w * w);  
    }
    
};

struct DecisionEngine {
    struct State {
        enum Type {
            Iddle,
            Move,
            Bust,
            Deliver,
            Stun,
            Stuned,
        };

        explicit State(int id, Type type, int targetId = -1, Point targetPoint = Point())
            : id(id)
            , type(type)
            , targetId(targetId)
            , targetPoint(targetPoint)
            , timeToLoad(0)
        {}
        int id;
        Type type;
        int  targetId;
        Point targetPoint;
        int timeToLoad;
    };

    const KnowledgeBase& m_kb;
    NavigationEngine m_nav;
    std::size_t stepCount;
    std::vector<State> m_busters0State;
    bool m_assignedKeyPoints[NB_KEYPOINTS];

    explicit DecisionEngine(const KnowledgeBase& kb)
      : m_kb(kb)
      , m_nav(kb)
      , stepCount(0)
    {
        std::cerr << "[dec][initialize] size " << m_kb.m_bustersPerPlayer << std::endl; 
        std::fill(begin(m_assignedKeyPoints), end(m_assignedKeyPoints), false);
        m_busters0State.reserve(m_kb.m_bustersPerPlayer);
        for (auto b = m_kb.m_busters0.begin(); b != m_kb.m_busters0.end(); ++b) {
            m_busters0State.push_back(State(b->second.id, State::Move, -1, chooseNextMovePoint(b->second)));
        }
        std::sort(
            m_busters0State.begin(),
            m_busters0State.end(),
            [](const State& a, const State& b) { return a.id < b.id; });
    }

    void step() { 
        std::cerr << "[dec] ===============" << std::endl; 
        ++stepCount;
        for (auto b = m_busters0State.begin(); b != m_busters0State.end(); ++b) {
            std::cerr << "[dec][#" << b->id << "] process " << b->type << std::endl; 
            const Buster& buster = m_kb.m_busters0.find(b->id)->second;
            if (b->timeToLoad != 0)
                --(b->timeToLoad);
            switch (b->type) {    
                case State::Move:
                    onMove(buster, *b);
                    break;
                case State::Bust:
                    onBust(buster, *b);
                    break;
                case State::Deliver:
                    onDeliver(buster, *b);
                    break;
                case State::Stun:
                    onStun(buster, *b);
                    break;
                default:
                {
                    std::cerr << "[dec][#" << b->id << "]" << " is default" << std::endl; 
                    break;
                }    
            }          
        }
    }
    void onDeliver(const Buster& buster, State& s) {
        if (buster.state == Buster::State::Carry) {
            Point a(buster.x, buster.y);
            if (m_nav.distance(a, s.targetPoint) <= DELIVERY_RADIUS) {
                ActionProcessor::release();
                std::cerr << "[dec][#" << s.id << "] release" << std::endl; 
            } else {
                std::cerr << "[dec][#" << s.id << "] going home" << std::endl; 
                ActionProcessor::move(s.targetPoint.x, s.targetPoint.y);
            }
        }
        else {
            s.targetPoint.id = std::rand() % NB_KEYPOINTS;
            moveToNextPoint(buster, s);
        }
    }
    void onBust(const Buster& buster, State&s) {
        if (buster.state == Buster::State::Carry) {
            s.type = State::Deliver;
            if (m_kb.m_myTeamId == 0)
                s.targetPoint = g_home0;
            else
                s.targetPoint = g_home1;
            ActionProcessor::move(s.targetPoint.x, s.targetPoint.y);
        }
        else if (buster.state == Buster::State::Busting) {
            auto it = std::find_if(
                        m_kb.m_currentGhosts.begin(),
                        m_kb.m_currentGhosts.end(),
                        [&](const Ghost& g) { return g.id == s.targetId; } );
            if (it != m_kb.m_currentGhosts.end()/*canHum()*/) {
                ActionProcessor::bust(s.targetId);
                std::cerr << "[dec][#" << s.id << "] bust #" << s.targetId << std::endl; 
            }  else {
                moveToNextPoint(buster, s);
            }
        }
        else {
            moveToNextPoint(buster, s);
        }
    }
    void onStun(const Buster& buster, State& s) {
        s.timeToLoad = 20;
        onMove(buster, s);
    }
    void onMove(const Buster& buster, State& s) {
        std::cerr << "[dec][#" << s.id << "] pouet" << buster.id << " " <<  m_busters0State.begin()->id << std::endl;
        if (stepCount > 280 && buster.id == m_busters0State.begin()->id) {
            move(buster, s, m_kb.m_myTeamId ? g_camp_home0 : g_camp_home1);
            std::cerr << "[dec][#" << s.id << "] camp" << std::endl;
            return;
        }
        // if ghost reachable -> hunt
        int entityId = canHunt(buster);
        if (-1 != entityId) {
            s.type = State::Bust; 
            s.targetId = entityId;
            ActionProcessor::bust(entityId);
            std::cerr << "[dec][#" << s.id << "] bust #" << entityId << std::endl; 
            return;
        }
        // We can stun
        entityId = canStun(buster, s);
        if (-1 != entityId) {
            s.type = State::Stun; 
            ActionProcessor::stun(entityId);
            std::cerr << "[dec][#" << s.id << "] stun #" << entityId << std::endl; 
            return;
        }
        // else move to target
        if (! m_nav.hasReachTarget(buster, s.targetPoint)) { 
            s.type = State::Move; 
            ActionProcessor::move(s.targetPoint.x, s.targetPoint.y);
            std::cerr << "[dec][#" << s.id << "] still moving" << std::endl; 
            return;
        } 
        moveToNextPoint(buster, s);
        return;
    }
    void moveToNextPoint(const Buster& buster, State& s) {
        std::size_t idx = s.targetPoint.id;

        m_assignedKeyPoints[idx] = false;
        move(buster, s, chooseNextMovePoint(buster, idx));
    }
    void move(const Buster& buster, State& s, const Point& point) {
        s.targetPoint = point;
        s.type = State::Move;
        std::cerr << "[dec][#" << s.id << "] move to next keyoint" << std::endl; 
        
        ActionProcessor::move(s.targetPoint.x, s.targetPoint.y);
    }
    int canStun(const Buster& buster, const State& s) {
        int targetId = -1;
        std::cerr << "[dec][#" << s.id << "] s.timeToLoad "<< s.timeToLoad << std::endl; 
        if (0 == s.timeToLoad) {
            
            std::cerr << "[dec][#" << s.id << "] nb ennemy "<< m_kb.m_currentEnnemies.size() << std::endl; 
            Point a(buster.x, buster.y);
            int minDist = std::numeric_limits<int>::max();
            for (auto it = m_kb.m_currentEnnemies.begin(); it != m_kb.m_currentEnnemies.end(); ++it) {
                Point b(it->x, it->y);
                auto d = m_nav.distance(a, b);
                if (d < minDist && m_nav.isInStunableRadius(d)) {
                    targetId = it->id;
                    minDist = d;
                }
            }
        }
        return targetId;   
    }
    int canHunt(const Buster& buster) const {
        int targetId = -1;
        Point a(buster.x, buster.y);
        int minDist = std::numeric_limits<int>::max();
        for (auto it = m_kb.m_currentGhosts.begin(); it != m_kb.m_currentGhosts.end(); ++it) {
            Point b(it->x, it->y);
            auto d = m_nav.distance(a, b);
            if (d < minDist && m_nav.isInBustableRadius(d)) {
                targetId = it->id;
                minDist = d;
            }
        }
        return targetId;        
    }
    Point chooseNextMovePoint(const Buster& b, std::size_t current = NB_KEYPOINTS) {
        std::size_t idx = current;
        if (idx == NB_KEYPOINTS) {
            idx = 4 * (std::rand() % 4);
        }
        else {
            idx = (idx + 1) % NB_KEYPOINTS;
        }
//        while (m_assignedKeyPoints[idx])
//            idx = (idx + 1) % NB_KEYPOINTS;
        m_assignedKeyPoints[idx] = true;
        return g_keypoints[idx];
    }
};

/**
 * Send your busters out into the fog to trap ghosts and bring them home!
 **/
int main()
{
    std::srand(std::time(0));
    
    KnowledgeBase kb;
    kb.step();
    DecisionEngine dec(kb);
    dec.step();
   
    // game loop
    while (1) {
        kb.step();
        dec.step();
    }
};