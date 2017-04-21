
#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace std;

#define LOG(x) 
//#define LOG(x) cerr << x << endl;

static const std::size_t NB_FACTORY_MAX = 15;
static const std::string ENTITY_TYPE_FACTORY = "FACTORY";
static const std::string ENTITY_TYPE_TROOP = "TROOP";

static const std::size_t UPGRADE_COST = 10;

static const double W_DISTANCE = 9;
static const double W_PROD = 2;
static const double W_ENNEMY = 1.2;
static const double W_BOMB_TRIGGER = 2;

struct Faction {
   enum Type {
      Neutral,
      Ally,
      Ennemy,
      Unknown
   };
   static std::string toString(int t) {
      switch (t) {
      case Ally:      return "Ally";
      case Ennemy:    return "Ennemy";
      case Neutral:
      default:        return "Neutral";
      }

   }
   static Type fromInt(int t) {
      switch (t) {
      case 1: return Ally;
      case -1: return Ennemy;
      default: return Neutral;
      }
   }
};
ostream& operator<<(ostream& os, const Faction::Type& t) {
   return os << Faction::toString(t);
}

struct Factory {
   Factory()
      : m_id(-1), m_nbCyborgs(0), m_prodFactor(0), m_faction(Faction::Neutral) {}
   Factory(int id, int nbCyborgs, Faction::Type faction, int prodFactor)
      : m_id(id), m_nbCyborgs(nbCyborgs), m_prodFactor(prodFactor), m_faction(faction) {}
   bool isAlly() const { return m_faction == Faction::Ally; };
   bool isEnnemy() const { return m_faction == Faction::Ennemy; };
   bool isNeutral() const { return m_faction == Faction::Neutral; };
   int m_id;
   int m_nbCyborgs;
   int m_prodFactor;
   Faction::Type m_faction;
};
typedef std::vector<Factory> T_Factories;

struct Troop {
   Troop(int targetId) : m_targetId(targetId)
   {
      m_nbCyborgs[Faction::Neutral] = 0;
      m_nbCyborgs[Faction::Ally] = 0;
      m_nbCyborgs[Faction::Ennemy] = 0;
   }
   int m_targetId;
   int m_nbCyborgs[Faction::Unknown];
};
typedef std::vector<Troop> T_Troops;

struct Knowledge {
private:
   Knowledge(const Knowledge& o)
      : m_localKb(), m_factories(o.m_factories), m_troops(o.m_troops)
      , m_distances(o.m_distances), m_availableBombs(o.m_availableBombs)
      , m_safeDistances(o.m_safeDistances), m_nbTotalCyborgs(o.m_nbTotalCyborgs)
   {
      m_bombTargetId[0] = o.m_bombTargetId[0];
      m_bombTargetId[1] = o.m_bombTargetId[1];
   }
   void operator=(const Knowledge& o) {
      m_factories = o.m_factories;
      m_troops = o.m_troops;
      m_availableBombs = o.m_availableBombs;
      m_nbTotalCyborgs = o.m_nbTotalCyborgs;
      m_bombTargetId[0] = o.m_bombTargetId[0];
      m_bombTargetId[1] = o.m_bombTargetId[1];
      m_safeDistances = o.m_safeDistances;
   }
   std::auto_ptr<Knowledge> m_localKb;


public:
   T_Factories m_factories;
   T_Troops m_troops;
   std::vector<std::vector<double> > m_distances;
   std::vector<std::vector<std::pair<double /*distance*/, int/*from*/> > > m_safeDistances;
   int m_availableBombs;
   int m_nbTotalCyborgs;
   int m_bombTargetId[2];

   Knowledge() {}
   void initialize() {
      LOG("======== init.knowledge");
      int factoryCount; // the number of factories
      cin >> factoryCount; cin.ignore();
      int linkCount; // the number of links between factories
      cin >> linkCount; cin.ignore();
      LOG("======== init.knowledge.factoryCount " << factoryCount);
      std::pair<int, int> distanceRange(std::numeric_limits<int>::max(), std::numeric_limits<int>::min());
      m_distances.resize(factoryCount);
      for (int i = 0; i < factoryCount; i++) {
         m_distances[i].resize(factoryCount);
         for (int j = 0; j < factoryCount; j++)
            m_distances[i][j] = std::numeric_limits<int>::max();
      }
      for (int i = 0; i < linkCount; i++) {
         int factory1, factory2, distance;
         cin >> factory1 >> factory2 >> distance; cin.ignore();

         m_distances[factory1][factory2] = distance;
         m_distances[factory2][factory1] = distance;
         distanceRange.first = std::min(distanceRange.first, distance);
         distanceRange.second = std::max(distanceRange.second, distance);
         LOG(factory1 << "->" << factory2 << ":" << distance);
      }
      //
      m_availableBombs = 2;
      m_bombTargetId[0] = -1;
      m_bombTargetId[1] = -1;
      m_factories.resize(factoryCount);
      m_troops.reserve(factoryCount);
      for (int i = 0; i < factoryCount; ++i)
         m_troops.push_back(Troop(i));

      initializeSafeDistances();
      // normalize distance
      double normalizeRatio = 0.1 * (distanceRange.second - distanceRange.first);
      for (auto& di : m_distances) {
         for (auto& dj : di) {
            dj /= normalizeRatio;
         }
      }
      m_localKb.reset(new Knowledge(*this));

   }
   void terminate() {
      LOG("======== terminate.knowledge");
      m_localKb.reset();
      m_factories.clear();
   }
   Knowledge& getLocalKnowledge() const {
      return *m_localKb;
   }
   void step() {
      LOG("======== step.knowledge");
      LOG("======== step.knowledge.local.read");
      // flip/flop local kb
      {
         m_availableBombs = m_localKb->m_availableBombs;
         m_bombTargetId[0] = m_localKb->m_bombTargetId[0];
         m_bombTargetId[1] = m_localKb->m_bombTargetId[1];
      }
      // reset step independent data
      {
         m_nbTotalCyborgs = 0;
         for (int i = 0; i < m_troops.size(); ++i)
            m_troops[i] = Troop(i);
         int entityCount = 0; // the number of entities (e.g. factories and troops)
         cin >> entityCount; cin.ignore();
         for (int i = 0; i < entityCount; i++) {
            string entityType;
            int entityId, arg1, arg2, arg3, arg4, arg5;
            cin >> entityId >> entityType >> arg1 >> arg2 >> arg3 >> arg4 >> arg5; cin.ignore();

            if (entityType == ENTITY_TYPE_FACTORY)
               updateFactory(entityId, Faction::fromInt(arg1), arg2, arg3);
            else /*if (entityId == ENTITY_TYPE_TROOP)*/
               updateTroop(entityId, Faction::fromInt(arg1), arg2, arg3, arg4, arg5);
         }
      }
      LOG("======== step.knowledge.local.write");
      *m_localKb = *this;
   }
   int getNbFactories() const { return static_cast<int>(m_factories.size()); };
   const Factory& getFactory(int idx) const { return m_factories[idx]; }
   Factory& getFactory(int idx) { return m_factories[idx]; }
   bool hasAvailableBomb() const { return m_availableBombs != 0; }
   bool isAlreadyTargeted(int idx) const { return m_bombTargetId[0] == idx || m_bombTargetId[1] == idx; }
   int getNextStepToGoTo(int srcId, int targetId) {
      if (srcId == targetId)
         return srcId;
      int nextStep;
      do {
         nextStep = targetId;
         targetId = m_safeDistances[srcId][targetId].second;
      } while (targetId != srcId);
      return nextStep;
   }
private:
   void updateFactory(int entityId, Faction::Type faction, int nbCyborgs, int prodFactor) {
      LOG("+ update factory: " << entityId << " " << nbCyborgs << " " << faction << " " << prodFactor);
      m_factories[entityId] = Factory(entityId, nbCyborgs, faction, prodFactor);
      m_nbTotalCyborgs += nbCyborgs;
   }
   void updateTroop(int entityId, Faction::Type faction, int srcFactoryId, int dstFactoryId, int nbCyborgs, int distance) {
      LOG("+ update troop: " << faction << " " << entityId << " " << srcFactoryId << "->" << dstFactoryId << " (" << nbCyborgs << ")");
      m_troops[dstFactoryId].m_nbCyborgs[faction] += nbCyborgs;
      m_nbTotalCyborgs += nbCyborgs;
   }
   void initializeSafeDistances() {
      // FloydWarshall
      m_safeDistances.resize(m_factories.size());
      for (auto i = 0; i < m_safeDistances.size(); ++i) {
         m_safeDistances[i].resize(m_factories.size());
         for (auto j = 0; j < m_safeDistances.size(); ++j) {
            m_safeDistances[i][j] = std::make_pair(m_distances[i][j], i);
            if (i == j)
               m_safeDistances[i][j] = std::make_pair(0, i);
         }
      }
      for (auto k = 0; k < m_safeDistances.size(); ++k) {
         for (auto i = 0; i < m_safeDistances.size(); ++i) {
            for (auto j = 0; j < m_safeDistances.size(); ++j) {
               auto distance = m_safeDistances[i][k].first + m_safeDistances[k][j].first;
               if (distance < m_safeDistances[i][j].first) {
                  m_safeDistances[i][j].first = distance;
                  m_safeDistances[i][j].second = k;
               }
            }
         }
      }
      /*std::cerr << "##################" << std::endl;
      for (auto i = 0; i < m_safeDistances.size(); ++i) {
      for (auto j = 0; j < m_safeDistances.size(); ++j) {
      std::cerr << "# " << i <<"->" <<j << " : " << m_safeDistances[i][j].first << " from #" << m_safeDistances[i][j].second << std::endl;
      }
      }
      std::cerr << "##################" << std::endl;*/
   }
};

struct Action {
   enum Type {
      Wait,
      Move,
      Bomb,
      IncrementProd
   };
   struct Order {
      Order(Type type, int srcFactoryId, int dstFactoryId = 0, int nbCyborgs = 0)
         : m_type(type), m_srcId(srcFactoryId), m_dstId(dstFactoryId), m_nbCyborgs(nbCyborgs) {}
      Type m_type;
      int m_srcId;
      int m_dstId;
      int m_nbCyborgs;
   };

   void initialize() {}
   void terminate() { m_orders.clear(); }
   void pushOrder(const Order& order) {
      LOG("+ pushOrder: " << order.m_type << " " << order.m_srcId << " " << order.m_dstId << " " << order.m_nbCyborgs);
      m_orders.push_back(order);
   }
   void step() {
      LOG("======== step.action ============");
      if (m_orders.empty()) {
         doWait();
      }
      else {
         int orderCount = 0;
         std::vector<Order> unfinishedOrders;
         for (int i = 0; i < m_orders.size(); ++i) {
            auto& o = m_orders[i];
            if (orderCount++ > 0)
               cout << ";";
            if (!doOrder(o))
               unfinishedOrders.push_back(o);
         }
         m_orders.clear();
         m_orders.swap(unfinishedOrders);
      }

      cout << std::endl;
   }

private:
   void doWait() {
      std::cout << "WAIT";
   }
   bool doOrder(const Order& o) {
      if (o.m_type == Move)
         std::cout << "MOVE " << o.m_srcId << " " << o.m_dstId << " " << o.m_nbCyborgs;
      else if (o.m_type == Bomb)
         std::cout << "BOMB " << o.m_srcId << " " << o.m_dstId;
      else if (o.m_type == IncrementProd)
         std::cout << "INC " << o.m_srcId;
      return true;
   }
   std::vector<Order> m_orders;
};

struct IStrategy {
   virtual void operator()() = 0;
};

struct BestProdStrategy : public IStrategy {
   BestProdStrategy(const Knowledge& kb, Action& action)
      : m_kb(kb), m_action(action), m_step(0) {}
   virtual void operator()() override {
      ++m_step;
      Knowledge& localKb = m_kb.getLocalKnowledge();
      auto allies = getFactories([](const Factory& f) { return f.isAlly(); });

      std::set<int> excludedTarget;

      // INC BEHAVIOR
      {
         auto alliesSortedBySafety = allies;
         std::sort(alliesSortedBySafety.begin(), alliesSortedBySafety.end(),
            [&](const Factory& a, const Factory& b) { return findClosestFactoryId(a.m_id, Faction::Ennemy) >= findClosestFactoryId(b.m_id, Faction::Ennemy); });
         for (auto& ally : alliesSortedBySafety) {
            auto nbCb = getDiscountedNbCyborgs(ally.m_id, Faction::Ally);
            if (ally.m_nbCyborgs >= UPGRADE_COST && nbCb >= UPGRADE_COST) {
               m_action.pushOrder(Action::Order(Action::IncrementProd, ally.m_id));
               ally.m_nbCyborgs -= UPGRADE_COST;
            }
         }
      }
      // BOMB BEHAVIOR
      if (localKb.hasAvailableBomb()) {
         int targetId = getBombId();
         if (targetId != -1 && !localKb.isAlreadyTargeted(targetId)) {
            auto srcId = findClosestFactoryId(targetId, Faction::Ally);
            if (srcId != -1) {
               m_action.pushOrder(Action::Order(Action::Bomb, srcId, targetId));
               --localKb.m_availableBombs;
               localKb.m_bombTargetId[localKb.m_availableBombs - 1] = targetId;
            }
         }
      }
      // MOVE ATTACK BEHAVIOR
      {
         auto scores = getDecisionAttackScores();
         if (!scores.empty()) {
            for (auto& s : scores) {
               auto& targetId = s.first;
               LOG("+ look to colonize " << targetId);
               auto nbEnnemies = std::max(getNbCyborgs(targetId, Faction::Ennemy), getNbCyborgs(targetId, Faction::Neutral));
               auto nbAllies = getNbCyborgs(targetId, Faction::Ally);
               auto targetProdFactor = localKb.m_factories[targetId].m_prodFactor;
               if (nbAllies <= nbEnnemies + targetProdFactor) {
                  int nbCyborgsToSent = 1 + targetProdFactor + (nbEnnemies - nbAllies);
                  std::sort(allies.begin(), allies.end(), [&](const Factory& a, const Factory& b) { return localKb.m_safeDistances[targetId][a.m_id].first < localKb.m_safeDistances[targetId][b.m_id].first; });
                  for (auto& ally : allies) {
                     bool isBombed = localKb.isAlreadyTargeted(ally.m_id);
                     auto nbCyborgsAvailable = ally.m_nbCyborgs - getNbCyborgs(ally.m_id, Faction::Ennemy) + getNbCyborgs(ally.m_id, Faction::Ally); // TODO TODO: remove the one coming from ennemy and add ally
                     auto pathToGoTo = localKb.getNextStepToGoTo(ally.m_id, targetId);
                     if (isBombed) {
                        auto v = ally.m_nbCyborgs;
                        m_action.pushOrder(Action::Order(Action::Move, ally.m_id, pathToGoTo, v));
                        ally.m_nbCyborgs -= v;
                        nbCyborgsToSent -= v;
                     }
                     else if (nbCyborgsAvailable > 0 && pathToGoTo == targetId) {
                        auto v = ally.m_nbCyborgs;
                        m_action.pushOrder(Action::Order(Action::Move, ally.m_id, pathToGoTo, v));
                        ally.m_nbCyborgs -= v;
                        nbCyborgsToSent -= v;
                     }
                     if (nbCyborgsToSent < 0)
                        break;
                  }
               }
            }
         }
      }
      // MOVE SUPPORT BEHAVIOR
      // Scoring From: 
      //  discount cbg           >
      //  distance to ennemy     >
      //  nb prod                <
      // Scoring To:
      //  reverse(From)
      if (true) {
         auto scores = getDecisionSupportScores();
         if (!scores.empty()) {
            auto first = 0;
            auto last = scores.size() - 1;
            auto mid = scores.size();
            while (first < last && mid > 0) {
               auto srcId = scores[first].first;
               auto targetId = scores[last].first;
               auto pathToGoTo = localKb.getNextStepToGoTo(srcId, targetId);
               if (localKb.m_factories[srcId].m_prodFactor == 3) {
                  LOG("- cover " << pathToGoTo << " from " << srcId);
                  auto& v = localKb.m_factories[srcId].m_nbCyborgs;
                  m_action.pushOrder(Action::Order(Action::Move, srcId, pathToGoTo, v));
                  localKb.m_factories[srcId].m_nbCyborgs = 0;
               }
               ++first;
               --last;
               mid /= 2;
            }
         }
      }
   }
   // *********** UTILITIES *********** //
   int getDiscountedNbCyborgs(int targetId, Faction::Type faction) const {
      Knowledge& localKb = m_kb.getLocalKnowledge();
      auto res = m_kb.m_factories[targetId].m_nbCyborgs + getNbCyborgs(targetId, faction);
      res -= getNbCyborgs(targetId, faction == Faction::Ally ? Faction::Ennemy : Faction::Ally);
      return res;
   }
   int getNbCyborgs(int targetId, Faction::Type faction) const {
      Knowledge& localKb = m_kb.getLocalKnowledge();
      int res = localKb.m_troops[targetId].m_nbCyborgs[faction];
      if (localKb.m_factories[targetId].m_faction == faction)
         res = localKb.m_factories[targetId].m_nbCyborgs;
      return res;
   }
   template<typename Predicate>
   std::vector<Factory> getFactories(Predicate p) const {
      std::vector<Factory> res;
      const Knowledge& localKb = m_kb.getLocalKnowledge();
      std::copy_if(localKb.m_factories.begin(), localKb.m_factories.end(), std::back_inserter(res), p);
      return res;
   }
   int findClosestFactoryId(int targetId, Faction::Type faction) const {
      const Knowledge& localKb = m_kb.getLocalKnowledge();
      const auto& distancesFromTarget = localKb.m_distances[targetId];
      auto allies = getFactories([&](const Factory& f) { return f.m_faction == faction; });
      auto id = -1;
      auto minD = std::numeric_limits<double>::max();
      for (auto& ally : allies) {
         auto d = distancesFromTarget[ally.m_id];
         if (d <= minD) {
            minD = d;
            id = ally.m_id;
         }
      }
      return id;
   }
   int getBombId() const {
      const Knowledge& localKb = m_kb.getLocalKnowledge();

      auto ennemies = getFactories([](const Factory& f) { return f.isEnnemy(); });
      auto allies = getFactories([](const Factory& f) { return f.isAlly(); });
      auto maxEnnemyCbg = 0;
      auto maxAllyCbg = 0;
      auto ennemyId = 0;
      for (auto& ally : allies)
         maxAllyCbg = std::max(maxAllyCbg, ally.m_nbCyborgs);
      for (auto& ennemy : ennemies) {
         auto futureNbCyborg = getDiscountedNbCyborgs(ennemy.m_id, Faction::Ennemy);
         if (maxEnnemyCbg <= futureNbCyborg) {
            maxEnnemyCbg = futureNbCyborg;
            ennemyId = ennemy.m_id;
         }
      }
      LOG("-> score to bomb #" << localKb.m_availableBombs << " on " << ennemyId << " " << maxAllyCbg << " : " << maxEnnemyCbg);
      if (ennemies.size() == 1 && allies.size() == 1 && localKb.m_availableBombs == 2)
         return ennemyId;
      if (maxEnnemyCbg > W_BOMB_TRIGGER * maxAllyCbg)
         return ennemyId;
      return -1;
   }
   double getMeanDistanceFromFaction(int srcId, Faction::Type faction) const {
      const Knowledge& localKb = m_kb.getLocalKnowledge();
      auto targets = getFactories([&](const Factory& f) { return faction == f.m_faction; });
      if (targets.empty())
         return 10;
      double distance = 0;
      for (auto& f : targets) {
         distance += localKb.m_safeDistances[srcId][f.m_id].first;
      }
      return distance / targets.size();
   }
private:
   // *********** ATTACK DECISION *********** //
   std::vector<std::pair<int, double> > getDecisionAttackScores() const {
      auto allies = getFactories([](const Factory& f) { return f.isAlly(); });
      auto others = getFactories([](const Factory& f) { return !f.isAlly(); });
      LOG("+ computing attack scores...");
      std::vector<std::pair<int, double> > scores;
      for (auto& target : others) {
         scores.push_back(std::make_pair(target.m_id, computeAttackValue(target, allies)));
      }
      std::sort(scores.begin(), scores.end(), [&](const std::pair<int, double>& a, const std::pair<int, double>& b) { return a.second >= b.second; });
      return scores;
   }
   double computeAttackValue(const Factory& target, const T_Factories& allies) const {
      const Knowledge& localKb = m_kb.getLocalKnowledge();
      const auto& distancesFromTarget = localKb.m_distances[target.m_id];
      double bombFactor = localKb.isAlreadyTargeted(target.m_id) ? 0 : 1;
      double factionScore = target.isEnnemy() ? W_ENNEMY : 1;
      double prodScore = 0.25 + W_PROD * target.m_prodFactor;
      double distanceScore = 0;
      for (auto& ally : allies) {
         auto d = distancesFromTarget[ally.m_id];
         distanceScore += W_DISTANCE / d;
      }
      if (!allies.empty())
         distanceScore /= allies.size();
      auto score = bombFactor* factionScore * prodScore * distanceScore;
      LOG("-> score to attack #" << target.m_id << " (" << bombFactor << " # " << factionScore << " # " << prodScore << " # " << distanceScore << ") --> " << score);
      return score;
   }
   // *********** SUPPORT DECISION *********** //
   std::vector<std::pair<int, double> > getDecisionSupportScores() const {
      auto allies = getFactories([](const Factory& f) { return f.isAlly(); });
      auto ennemies = getFactories([](const Factory& f) { return !f.isAlly(); });
      LOG("+ computing support scores...");
      std::vector<std::pair<int, double> > scores;
      for (auto& ally : allies) {
         scores.push_back(std::make_pair(ally.m_id, computeSupportValue(ally, ennemies)));
      }
      std::sort(scores.begin(), scores.end(), [&](const std::pair<int, double>& a, const std::pair<int, double>& b) { return a.second >= b.second; });
      return scores;
   }
   double computeSupportValue(const Factory& src, const T_Factories& ennemies) const {
      const Knowledge& localKb = m_kb.getLocalKnowledge();

      double prodScore = 0.1 + 5 * src.m_prodFactor / W_PROD;
      double distanceToEnnemies = getMeanDistanceFromFaction(src.m_id, Faction::Ennemy);
      double discountedCbg = 1 + 10 * (static_cast<double>(getDiscountedNbCyborgs(src.m_id, Faction::Ally)) / std::max(1, m_kb.m_nbTotalCyborgs));
      auto score = prodScore * distanceToEnnemies * discountedCbg;
      LOG("-> score to support #" << src.m_id << " (" << " # " << prodScore << " # " << distanceToEnnemies << " # " << discountedCbg << ") --> " << score);
      return score;
   }

   const Knowledge& m_kb;
   Action& m_action;
   int m_step;
};

struct Decision {
   enum Strategy {
      Random,
      BestProd
   };
   Decision(const Knowledge& kb, Action& action, Strategy strategy = BestProd)
      : m_kb(kb), m_action(action), m_strategy()
   {
      switch (strategy) {
      case Random:
      case BestProd:
      default:
      {
         m_strategy = std::unique_ptr<IStrategy>(new BestProdStrategy(m_kb, m_action));
         break;
      }
      }
   }
   void initialize() {}
   void terminate() {}
   void step() {
      LOG("======== step.decision ============");
      (*m_strategy)();
   }

private:
   const Knowledge& m_kb;
   Action& m_action;
   std::unique_ptr<IStrategy> m_strategy;
};

struct Simulation {
   Simulation()
      : m_action(), m_kb(), m_dec(m_kb, m_action)
   {
      m_action.initialize();
      m_kb.initialize();
      m_dec.initialize();
   }
   ~Simulation() {
      m_dec.terminate();
      m_kb.terminate();
      m_action.terminate();
   }
   void step() {
      LOG("======== step ============");
      m_kb.step();
      m_dec.step();
      m_action.step();
   }
private:
   Action m_action;
   Knowledge m_kb;
   Decision m_dec;
};
/**
* Auto-generated code below aims at helping you parse
* the standard input according to the problem statement.
**/
int main()
{
   Simulation sim;
   // game loop
   while (1) {
      sim.step();
   }
}