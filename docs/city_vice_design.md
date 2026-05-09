# City Vice -- Design Document

**Feature name**: City Vice (Urban Crime & Unrest)
**Version**: 1.0
**Status**: Proposal
**Date**: 2026-03-14

---

## 1. Overview

### The Player Fantasy

As your transport empire fuels urban growth, cities develop a darker side. Overcrowded metropolises with poor service coverage experience civil unrest -- vandalism to stations, vehicle breakdowns from sabotage, and occasionally the demolition of neglected buildings. The player who thoughtfully services their cities, maintains good authority ratings, and invests in civic safety finds their cities thriving and resilient. The player who extracts profits from cities without reinvesting faces mounting disruption.

City Vice is **not** a punishment system. It is a risk/reward layer that makes city growth a double-edged sword: larger cities generate more cargo revenue, but they also require more attentive service to remain stable. This creates decisions:

- *Do I grow this city aggressively for cargo, knowing I will need to service it well to avoid unrest?*
- *Should I spread my routes across many small towns or concentrate on a few large ones?*
- *Is it worth funding a police station to protect my infrastructure in this metropolis?*

### Experience Arc

| Game Phase | Population Range | Vice Intensity | Player Experience |
|---|---|---|---|
| Early game (1950-1970) | 0-1000 | None | Free from vice; learn the mechanic through news messages in other towns |
| Mid game (1970-1990) | 1000-5000 | Light | Occasional minor vandalism; player starts budgeting for repairs |
| Late game (1990-2020) | 5000-50000 | Moderate | Regular events; player actively mitigates through service quality and civic investment |
| Endgame (2020+) | 50000+ | Heavy | Metropolis management becomes a core challenge alongside logistics |

---

## 2. Interaction Map

The following table documents every OpenTTD system that City Vice touches, the nature of the interaction, and the direction of influence.

| System | Interaction | Direction | Notes |
|---|---|---|---|
| Town population (`Town::cache.population`) | Population is the primary input to vice probability | Vice reads population | No modification to population growth logic itself |
| Town growth (`TownTickHandler`, `GrowTown`) | Vice can destroy houses, reducing `num_houses` and slowing growth indirectly | Vice modifies town state | Destroyed houses are rebuilt by normal growth -- no death spiral |
| Town authority rating (`Town::ratings[]`) | Good ratings reduce vice probability; vice events reduce ratings for poorly-serviced companies | Bidirectional | Ratings range: -1000 to +1000 |
| Company rating with town | Companies with RATING_GOOD (+400) or above get reduced vice impact | Vice reads rating | Encourages players to maintain reputation |
| Disaster system (`disaster_vehicle.cpp`) | City Vice runs on a parallel timer, not through the disaster vehicle system | Independent | Different design: vice is per-town, disasters are global |
| Vehicles (road, rail) | Vehicles in town radius can suffer breakdowns from vandalism | Vice modifies vehicle state | Uses existing `breakdown_ctr` / `breakdown_delay` mechanism |
| Stations | Stations in town can have rating temporarily reduced | Vice modifies station state | Simulates vandalism reducing service quality |
| Economy (`ExpensesType`) | Repair costs charged under `EXPENSES_PROPERTY` | Vice creates expenses | Proportional to damage, never catastrophic |
| News system | All vice events generate news items | Vice creates news | Transparency: player always knows what happened and why |
| Town actions (`TownAction`) | New action: "Fund Policing" to reduce vice | Vice adds new action | Cost comparable to "Fund Buildings" |
| Settings | Master toggle, intensity slider, per-town override via GS | Vice reads settings | Full player control |
| AI/GS Script API | Events broadcast to AI/GS so they can react | Vice creates script events | AI companies need awareness |

---

## 3. Core Mechanics

### 3.1 Vice Level Calculation

Each town has a **vice level** calculated monthly. This is a value from 0 to 1000, analogous to the rating system. It determines the probability and severity of vice events.

#### Formula (Integer Math Only)

```
base_vice = (population * 100) / (population + 5000)
```

This is a hyperbolic curve that asymptotes at 100. It rises steeply for small cities and flattens for large ones.

**Explanation**: For a town of 1000 people, `base_vice = 1000 * 100 / 6000 = 16`. For a town of 50000, `base_vice = 50000 * 100 / 55000 = 90`. The denominator `population + 5000` ensures the curve never reaches 100 and small towns are largely unaffected.

#### Service Modifier

Good transport service reduces vice. The service modifier is based on the percentage of cargo transported from the town.

```
pct_transported = town->GetPercentTransported(passengers)  // 0-255 scale
service_modifier = (pct_transported * 60) / 256            // 0-60 range

// Higher transport percentage = lower vice
adjusted_vice = max(0, base_vice - service_modifier)
```

A town with 80% passenger transport coverage gets `service_modifier = (204 * 60) / 256 = 47`, reducing vice substantially.

#### Authority Rating Modifier

The best company rating in the town further modifies vice. Good relationships with the town reduce unrest.

```
best_rating = max(ratings[0..N])  // Best company rating in town, -1000 to +1000

// Map rating to a modifier: +1000 rating gives -20 vice, -1000 rating gives +10 vice
rating_modifier = -(best_rating * 20) / 1000  // Range: -20 to +20
// Negative modifier = reduction (good), positive = increase (bad)

final_vice = clamp(adjusted_vice + rating_modifier, 0, 100)
```

#### Complete Lookup Table

| Population | Base Vice | Svc Mod (50% transported) | Svc Mod (80% transported) | Rating Mod (Excellent +800) | Final Vice (50% svc, Good rating) | Final Vice (80% svc, Excellent rating) |
|---|---|---|---|---|---|---|
| 100 | 1 | 23 | 47 | -16 | 0 | 0 |
| 300 | 5 | 23 | 47 | -16 | 0 | 0 |
| 500 | 9 | 23 | 47 | -16 | 0 | 0 |
| 1000 | 16 | 23 | 47 | -16 | 0 | 0 |
| 2000 | 28 | 23 | 47 | -16 | 0 | 0 |
| 3000 | 37 | 23 | 47 | -16 | 0 | 0 |
| 5000 | 50 | 23 | 47 | -16 | 11 | 0 |
| 8000 | 61 | 23 | 47 | -16 | 22 | 0 |
| 10000 | 66 | 23 | 47 | -16 | 27 | 3 |
| 15000 | 75 | 23 | 47 | -16 | 36 | 12 |
| 20000 | 80 | 23 | 47 | -16 | 41 | 17 |
| 30000 | 85 | 23 | 47 | -16 | 46 | 22 |
| 50000 | 90 | 23 | 47 | -16 | 51 | 27 |
| 100000 | 95 | 23 | 47 | -16 | 56 | 32 |

**Key insight**: Well-serviced towns with good ratings are nearly immune to vice below 5000 population. Even metropolises of 50k+ have manageable vice levels (~27) with good service, compared to neglected cities (~90). This rewards engagement without making vice trivial.

### 3.2 Vice Event Trigger

Vice events are checked monthly, during the existing `_economy_towns_monthly` timer callback. This avoids adding new timers and integrates cleanly.

#### Monthly Event Check

```
// Roll for whether a vice event occurs this month
roll = RandomRange(1000)

if (roll < final_vice * vice_intensity_setting) {
    TriggerViceEvent(town)
}
```

Where `vice_intensity_setting` is a user-configurable multiplier (default 10, range 0-20). At default settings:

| Final Vice Level | Monthly Probability | Expected Months Between Events | Expected Events per Game Year |
|---|---|---|---|
| 0 | 0.0% | Never | 0 |
| 5 | 5.0% | 20 months | 0.6 |
| 10 | 10.0% | 10 months | 1.2 |
| 20 | 20.0% | 5 months | 2.4 |
| 30 | 30.0% | 3.3 months | 3.6 |
| 50 | 50.0% | 2 months | 6.0 |
| 75 | 75.0% | 1.3 months | 9.0 |
| 100 | 100.0% | 1 month | 12.0 |

### 3.3 Vice Event Types

When a vice event triggers, the type is selected randomly weighted by population tier.

| Event Type | Weight (Pop < 5k) | Weight (Pop 5k-15k) | Weight (Pop 15k+) | Effect |
|---|---|---|---|---|
| **Petty Vandalism** | 60 | 40 | 20 | Station rating -15 for 1 random station in town |
| **Vehicle Sabotage** | 25 | 25 | 20 | 1 random vehicle in town radius gets breakdown |
| **Arson** | 10 | 20 | 25 | 1 house destroyed |
| **Riot** | 5 | 10 | 20 | 1-3 houses destroyed + 1 station rating hit |
| **Crime Wave** | 0 | 5 | 15 | All stations in town get rating -10 for 3 months |

Selection algorithm (integer math):
```
total_weight = sum of all weights for this population tier
roll = RandomRange(total_weight)
// Walk through events, subtracting weights until roll < 0
```

### 3.4 Event Effects in Detail

#### Petty Vandalism
- Selects a random station owned by any company within the town radius
- Reduces the station's cargo rating by 15 points (out of 255 max) for one affected cargo type
- Generates a minor news item: "Vandals target {station_name} in {town_name}"
- No lasting damage; rating recovers naturally

#### Vehicle Sabotage
- Selects a random road vehicle or train within the town's `squared_town_zone_radius[HZB_TOWN_EDGE]`
- Sets `breakdown_ctr = 2` and `breakdown_delay = 120` (existing breakdown mechanism)
- Only targets vehicles currently within town radius, not in depots
- Generates a news item: "Vehicle {vehicle_name} sabotaged in {town_name}"
- **Excluded**: Aircraft and ships (aircraft are at airports with security; ships are not in the urban area)

#### Arson
- Selects a random house tile within the town
- Calls `ClearTownHouse(t, tile)` -- the same function used by the existing disaster system and by normal town rebuilding
- Only targets completed houses (`IsHouseCompleted(tile)`) that satisfy `CanDeleteHouse(tile)`
- Protected buildings (churches, stadiums) are excluded
- Generates a news item with the explosion visual effect: "Fire destroys building in {town_name}"
- The town will naturally rebuild the house through its growth cycle

#### Riot
- Destroys 1 to 3 houses (count = `1 + RandomRange(clamp(population / 20000, 0, 2))`)
- Each house destroyed follows the same rules as Arson
- Additionally hits one random station with a -15 rating penalty
- Reduces all company ratings in this town by 50 points (simulates public anger at all companies)
- Generates a prominent news item: "Riots break out in {town_name}!"
- Creates explosion visual effects at destroyed house locations

#### Crime Wave
- Applies a -10 cargo rating penalty to all stations in the town
- Sets a `crime_wave_months = 3` counter on the town (new field)
- While active, station ratings in the town recover 50% slower
- Generates a news item: "Crime wave grips {town_name}"
- Does not destroy any buildings or vehicles directly

### 3.5 Buildings Affected Per Event -- Detailed Table

| Event | Population < 5000 | Population 5000-15000 | Population 15000-50000 | Population 50000+ |
|---|---|---|---|---|
| Petty Vandalism | 0 buildings | 0 buildings | 0 buildings | 0 buildings |
| Vehicle Sabotage | 0 buildings | 0 buildings | 0 buildings | 0 buildings |
| Arson | 1 building | 1 building | 1 building | 1 building |
| Riot | 1 building | 1 building | 1-2 buildings | 1-3 buildings |
| Crime Wave | 0 buildings | 0 buildings | 0 buildings | 0 buildings |

Formula for Riot building count:
```
riot_buildings = 1 + RandomRange(clamp(population / 20000, 0, 2))
```

| Population | Max Riot Buildings | Calculation |
|---|---|---|
| 5000 | 1 | 1 + RandomRange(0) = 1 |
| 10000 | 1 | 1 + RandomRange(0) = 1 |
| 20000 | 1-2 | 1 + RandomRange(1) |
| 40000 | 1-3 | 1 + RandomRange(2) |
| 60000+ | 1-3 | 1 + RandomRange(2) -- capped |

### 3.6 Anti-Death-Spiral Protections

These safeguards prevent vice from cascading into an unrecoverable situation:

1. **Cooldown**: After any vice event, the town gets a 2-month cooldown where no new events can trigger. This is tracked via a `vice_cooldown` counter decremented monthly.

2. **Population Floor**: Vice events cannot fire in towns below 1000 population. This protects early-game play entirely.

3. **Rebuilding**: Destroyed houses are rebuilt by the normal town growth cycle (`TileLoop_Town` and `GrowTown`). Well-serviced towns rebuild within 1-3 months.

4. **Severity Cap**: No single event can destroy more than 3 buildings. For context, even a large town with 500+ houses loses at most 0.6% of its buildings in the worst case.

5. **Rating Recovery**: Company ratings naturally increase by `RATING_GROWTH_UP_STEP` (+5) per month and `RATING_STATION_UP_STEP` (+12) per well-serviced station. A -50 rating hit from a Riot is recovered in ~4 months with a single well-serviced station.

6. **Service Safety Net**: Towns with >60% passenger transport coverage are nearly immune to vice at populations below 15,000. Even a minimal bus route provides significant protection.

---

## 4. Economic Modelling

### 4.1 Cost of Vice Events

| Event | Direct Cost to Player | Indirect Cost | Recovery Time |
|---|---|---|---|
| Petty Vandalism | None | ~2-5% revenue loss from affected station for 1 month | 1 month |
| Vehicle Sabotage | ~500-2000 repair (existing breakdown cost) | Lost revenue during breakdown (~1-3 days) | Immediate (vehicle auto-repairs) |
| Arson | None (town rebuilds) | Temporary population loss (~50-200 people) | 1-3 months |
| Riot | None direct | Population loss + station rating hit + company rating hit | 3-6 months |
| Crime Wave | None direct | ~5-10% revenue loss across all town stations for 3 months | 3 months |

### 4.2 Comparison with Existing Costs

| Cost/Loss Category | Typical Amount | Comparison |
|---|---|---|
| Vice event (typical, Petty Vandalism) | ~1,000-5,000 lost revenue | < 1 bus monthly running cost |
| Vice event (worst case, Riot) | ~10,000-30,000 lost revenue over recovery | < 1 train monthly running cost |
| Crime Wave (3 months sustained) | ~20,000-100,000 lost revenue | Comparable to 1 small advertising campaign |
| Fund Policing (new town action, monthly) | ~50,000 per use | Between "Fund Buildings" and "Buy Rights" |
| Disaster (Zeppelin crash, for comparison) | Airport blocked for months | Much more severe than any single vice event |
| Disaster (Small UFO, road vehicle destroyed) | ~20,000-100,000 vehicle replacement | Comparable to worst-case vice |

**Key finding**: Individual vice events are deliberately minor -- comparable to normal vehicle breakdowns or station rating fluctuations. Their impact comes from *frequency* in badly-managed cities. A neglected 50k metropolis might face 6+ events per year, accumulating into meaningful but never catastrophic economic drag.

### 4.3 Revenue Impact Analysis

Consider a player operating 5 bus routes and 2 train routes through a 20,000-population city generating approximately 300,000/year in passenger revenue.

| Scenario | Vice Events/Year | Revenue Impact | Net Revenue |
|---|---|---|---|
| Well serviced (80% transport, Excellent rating) | ~2 | -2% (~6,000) | 294,000 |
| Average service (50% transport, Good rating) | ~5 | -5% (~15,000) | 285,000 |
| Poor service (20% transport, Mediocre rating) | ~8 | -10% (~30,000) | 270,000 |
| Neglected (no service, Poor rating) | ~10+ | -15% (~45,000) | 255,000 |

The difference between excellent and neglected service is ~39,000/year -- roughly the cost of running one additional bus route. This creates a meaningful incentive to maintain service quality without making neglect catastrophic.

### 4.4 Mitigation Cost/Benefit

| Mitigation | Annual Cost | Vice Reduction | Break-Even Population |
|---|---|---|---|
| Add 1 bus route | ~15,000 running cost | Improves transport %, reducing vice by ~5-15 | ~3,000 (where vice begins to matter) |
| Fund Policing (new action) | ~50,000 per use, lasts 6 months | Reduces vice level by 30 for duration | ~15,000 (where cost is offset by prevented damage) |
| Improve rating (trees, advertising) | ~2,000-10,000 one-time | Rating modifier reduces vice by up to 20 | Always worthwhile if rating is low |
| Build statue | ~50,000 one-time | Permanent +1 to rating recovery, marginal vice reduction | ~10,000 |

---

## 5. Balance Analysis

### 5.1 Early Game (1950-1970)

| Factor | Assessment |
|---|---|
| Typical town size | 100-1000 |
| Vice level | 0 (population floor at 1000) |
| Player interaction with vice | None -- pure learning phase |
| Revenue at risk | 0 |

Early-game balance is unaffected. New players learn the game without any additional complexity. Vice is introduced gradually as towns grow organically past 1000 population.

### 5.2 Mid Game (1970-1990)

| Factor | Assessment |
|---|---|
| Typical town size | 1000-5000 |
| Vice level | 0-16 (base), 0-5 (well serviced) |
| Events per year | 0-1 |
| Dominant event types | Petty Vandalism (60%), Vehicle Sabotage (25%) |
| Revenue at risk | <2% |
| Player agency | Adding bus routes largely negates vice |

Mid-game introduces vice as a gentle nudge toward better service coverage. A player who has bus routes in their towns barely notices vice. A player who ignores towns entirely gets occasional reminders via news messages. The dominant event (Petty Vandalism) is essentially cosmetic at this stage.

### 5.3 Late Game (1990-2020)

| Factor | Assessment |
|---|---|
| Typical town size | 5000-30000 |
| Vice level | 20-50 (base), 5-20 (well serviced) |
| Events per year | 1-4 |
| Dominant event types | Mix of all types; Arson and Riot become possible |
| Revenue at risk | 2-10% |
| Player agency | Service quality + Fund Policing + Authority rating all matter |

Late-game is where vice becomes a genuine consideration. Players must decide whether to invest in civic infrastructure (policing, service coverage) or accept periodic disruption. The decision is meaningful because the cost of mitigation is comparable to the cost of damage -- there is no objectively "correct" answer, only tradeoffs.

### 5.4 Endgame (2020+)

| Factor | Assessment |
|---|---|
| Typical town size | 30000-100000+ |
| Vice level | 40-95 (base), 15-35 (well serviced) |
| Events per year | 2-6 (well managed) to 10+ (neglected) |
| Dominant event types | Riots and Crime Waves become regular for neglected cities |
| Revenue at risk | 5-15% |
| Player agency | Full toolkit needed; becomes a management layer alongside logistics |

Endgame metropolises are the feature's showcase. Managing a 100k city requires attention: regular policing, good service coverage, and high authority ratings. But even in the worst case, a neglected metropolis is still profitable -- it just makes 15% less than it could. The player is always better off having a large city than a small one.

### 5.5 Play Style Impact

| Play Style | Impact of Vice | Mitigation Difficulty |
|---|---|---|
| Passenger-focused (buses/trains) | Medium -- but good service coverage naturally mitigates | Low -- natural fit |
| Cargo/freight-focused | Low -- freight service still counts for transport % | Low |
| Aggressive growth (fund buildings) | High -- rapid growth creates high-vice cities faster | Medium -- need to keep up with service |
| Many small towns | Very Low -- most towns stay below vice threshold | None |
| Few large cities | Medium-High -- concentrated exposure but concentrated mitigation | Medium |
| Competitive multiplayer | Interesting -- vice creates another dimension of city management competition | Varies |

---

## 6. Alternatives Considered

### 6.1 Event Trigger: Population-Based vs. Growth-Rate-Based

| Criterion | Option A: Population-Based (CHOSEN) | Option B: Growth-Rate-Based |
|---|---|---|
| **Description** | Vice scales with absolute population | Vice scales with growth speed (fast growth = more vice) |
| **Predictability** | High -- player can look at population and know risk | Medium -- growth rate fluctuates and is less visible |
| **Thematic fit** | Strong -- larger cities have more crime (real-world pattern) | Moderate -- "growing pains" narrative is plausible but less intuitive |
| **Player agency** | Indirect -- player controls service quality, not population | Direct -- player can slow growth to reduce vice, but this conflicts with the goal of growing |
| **Interaction with "Fund Buildings"** | Neutral -- funding doesn't change vice directly | Negative -- funding increases growth rate, which would *increase* vice, making the action self-defeating |
| **Implementation complexity** | Simple -- population is always available | Moderate -- need to track growth rate over time |
| **Recommendation** | **Selected** | Rejected -- penalizing growth conflicts with game identity |

### 6.2 Damage Model: Building Destruction vs. Building Damage

| Criterion | Option A: Building Destruction (CHOSEN) | Option B: Building Damage (Reduced Production) |
|---|---|---|
| **Description** | Houses are fully removed via `ClearTownHouse()`, town rebuilds them | Houses enter "damaged" state, produce less cargo until repaired |
| **Visual clarity** | High -- empty tile is immediately obvious | Medium -- need new sprites for damaged state |
| **Code complexity** | Low -- reuses existing demolition and growth code | High -- new building state, new sprites, new cargo calculation path |
| **Recovery mechanism** | Automatic -- town growth rebuilds houses | Manual -- player or town must "repair" (new mechanic needed) |
| **Risk of frustration** | Low -- town fixes itself | Medium -- if repair is manual, adds tedious busywork |
| **Death spiral risk** | Low -- destroyed houses reduce population, which reduces vice | Medium -- damaged houses might still count toward population but produce less, not reducing vice |
| **Recommendation** | **Selected** | Rejected -- too much new machinery for marginal benefit |

### 6.3 Mitigation: Town Action vs. New Building Type

| Criterion | Option A: Town Action "Fund Policing" (CHOSEN) | Option B: New Building "Police Station" |
|---|---|---|
| **Description** | Pay money via town authority menu, reduces vice for 6 months | Build a special building that provides an ongoing vice reduction |
| **Player interaction** | Familiar -- same UI as existing town actions | Novel -- requires new building mechanics, placement rules |
| **Recurring cost** | Yes -- must re-fund every 6 months | One-time build cost + running cost |
| **Strategic depth** | Medium -- simple cost/benefit | High -- placement matters, covers a radius |
| **Code complexity** | Low -- add one entry to `_town_action_proc[]` | High -- new object type, radius calculation, running costs |
| **Balance risk** | Low -- easy to tune the cost | Medium -- permanent building might be too strong or too weak |
| **Recommendation** | **Selected for initial implementation** | Considered for future expansion |

### 6.4 Scope: Per-Town vs. Global Vice

| Criterion | Option A: Per-Town Vice (CHOSEN) | Option B: Global Vice Level |
|---|---|---|
| **Description** | Each town has its own independent vice level | One global vice level affecting all towns |
| **Granularity** | High -- player manages each city individually | Low -- blunt instrument, no targeting |
| **Thematic fit** | Strong -- crime is local | Weak -- all cities having the same crime rate is unrealistic |
| **Player agency** | High -- invest in specific trouble spots | Low -- can only change global policy |
| **Interaction with service quality** | Natural -- per-town service quality modifies per-town vice | Awkward -- what transport % applies globally? |
| **Recommendation** | **Selected** | Rejected -- too simplistic |

### 6.5 Vice Impact on Vehicles: Breakdown vs. Destruction

| Criterion | Option A: Breakdown (CHOSEN) | Option B: Vehicle Destruction |
|---|---|---|
| **Description** | Vehicle gets breakdown using existing mechanism | Vehicle is destroyed (like Small UFO) |
| **Severity** | Low -- vehicle auto-repairs, minor delay | Extreme -- total loss of vehicle + cargo |
| **Player frustration** | Low -- familiar, expected, recoverable | Very High -- losing a train to "crime" feels unfair |
| **Balance** | Natural -- breakdowns happen anyway | Requires heavy compensation to feel fair |
| **Consistency with existing game** | High -- breakdowns are routine | Low -- only the most severe disasters destroy vehicles |
| **Recommendation** | **Selected** | Rejected -- disproportionate to the "urban crime" theme |

---

## 7. Settings and Configuration

### 7.1 Game Settings

| Setting | Type | Default | Range | Location | Description |
|---|---|---|---|---|---|
| `economy.city_vice` | bool | false | on/off | Advanced Settings > Economy > Towns | Master toggle for the City Vice feature |
| `economy.city_vice_intensity` | uint8 | 10 | 0-20 | Advanced Settings > Economy > Towns | Multiplier for vice event probability. 0 = off, 10 = normal, 20 = aggressive |
| `economy.city_vice_min_population` | uint16 | 1000 | 300-10000 | Advanced Settings > Economy > Towns | Minimum town population before vice events can occur |
| `difficulty.city_vice_severity` | uint8 | 1 | 0-2 | Difficulty Settings | 0 = Mild (no Riots/Crime Waves), 1 = Normal, 2 = Harsh (all events, +50% frequency) |

### 7.2 Settings Interaction Matrix

| Vice Intensity | Severity: Mild | Severity: Normal | Severity: Harsh |
|---|---|---|---|
| 0 (Off) | No events | No events | No events |
| 5 (Low) | Rare Vandalism only | Rare, all minor types | Occasional, all types |
| 10 (Normal) | Occasional Vandalism, Sabotage | Normal frequency, all types | Frequent, all types |
| 15 (High) | Frequent Vandalism, Sabotage, Arson | Frequent, all types | Very frequent, all types |
| 20 (Max) | Very frequent minor events | Very frequent, all types | Constant pressure, all types |

### 7.3 Difficulty Severity Details

| Severity Level | Available Event Types | Frequency Modifier | Max Buildings per Riot | Cooldown |
|---|---|---|---|---|
| Mild (0) | Petty Vandalism, Vehicle Sabotage | x1 | N/A (no Riots) | 3 months |
| Normal (1) | All types | x1 | 3 | 2 months |
| Harsh (2) | All types | x1.5 (multiply final_vice by 3/2 before roll) | 4 | 1 month |

### 7.4 GameScript Override

The following GS functions allow scenario creators to control vice per-town:

```
GStown.SetViceLevel(town_id, level)    // Override calculated vice (0-100)
GStown.GetViceLevel(town_id)           // Read current vice level
GStown.SetViceCooldown(town_id, months) // Set event cooldown
GStown.TriggerViceEvent(town_id, type)  // Force a specific event
```

---

## 8. AI Considerations

### 8.1 AI Awareness

AI companies need to be notified of vice events affecting their assets. The following script events are broadcast:

| Event | Script Event Class | Parameters |
|---|---|---|
| Station vandalized | `ScriptEventStationVandalized` | station_id, town_id, rating_loss |
| Vehicle sabotaged | `ScriptEventVehicleSabotaged` | vehicle_id, town_id |
| Building destroyed | `ScriptEventBuildingDestroyed` | town_id, tile, num_buildings |
| Riot | `ScriptEventRiot` | town_id, num_buildings, stations_affected |
| Crime Wave started | `ScriptEventCrimeWave` | town_id, duration_months |
| Vice level changed | `ScriptEventViceLevelChanged` | town_id, new_level |

### 8.2 AI Response Strategy

The recommended AI response logic (for AI library authors):

```
Priority 1: If vice_level > 50 and company has stations in town:
    -> Fund Policing (if available and affordable)

Priority 2: If vice_level > 30 and transport_pct < 50%:
    -> Add bus route to increase service coverage

Priority 3: If vehicle_sabotaged event:
    -> No action needed (vehicle self-repairs)

Priority 4: If crime_wave event:
    -> Consider temporarily reducing service to affected town
    -> Or Fund Policing to end it early

Default: AI ignores vice below level 30 (events are too rare to matter)
```

### 8.3 AI Fairness

- AI companies are affected by vice events on the same terms as human players
- The Big UFO disaster already exempts AI-owned trains from targeting (`Company::IsHumanID`); City Vice does NOT follow this precedent. Vice is a town-level phenomenon that affects all companies equally
- AI companies can use Fund Policing through the existing town action command system
- The service modifier means AI companies that naturally service towns well are already protected

---

## 9. Visual and Audio Indicators

### 9.1 Town Window Additions

| Element | Location | Appearance | Condition |
|---|---|---|---|
| Vice level indicator | Town view window, below population | Bar gauge: green (0-20), yellow (21-50), orange (51-75), red (76-100) | Always visible when City Vice is enabled |
| Vice status text | Town view window, info section | "Crime level: Low/Moderate/High/Critical" | Always visible when City Vice is enabled |
| Policing active icon | Town view window, next to vice indicator | Shield icon (similar to "Fund Buildings" indicator) | When Fund Policing is active |
| Crime Wave warning | Town view window, flashing text | "Crime Wave in progress! ({months} months remaining)" | During Crime Wave events |

### 9.2 Map View

| Element | Appearance | Condition |
|---|---|---|
| Town name color tint | Normal = white, High vice = orange tint, Critical = red tint | When vice > 50 or > 75 |
| Vice overlay (optional) | Color-coded zones around town center showing vice intensity | Toggle in map legend, similar to town authority zone |

### 9.3 Visual Effects During Events

| Event | Visual Effect | Duration |
|---|---|---|
| Petty Vandalism | Small smoke puff at affected station | 2-3 seconds |
| Vehicle Sabotage | Existing breakdown smoke on vehicle | Until breakdown ends |
| Arson | `EV_EXPLOSION_SMALL` at house location (reuses existing effect) | ~5 seconds |
| Riot | `EV_EXPLOSION_LARGE` at first house + `EV_EXPLOSION_SMALL` at subsequent houses | ~8 seconds |
| Crime Wave start | Brief "alarm" effect at town center | ~3 seconds |

### 9.4 Audio

| Event | Sound | Existing Asset |
|---|---|---|
| Petty Vandalism | Glass breaking sound | New sound needed (or reuse `SND_2A_CRASH` at reduced volume) |
| Vehicle Sabotage | Breakdown sound | Existing vehicle breakdown sound |
| Arson / Riot | Explosion sound | `SND_12_EXPLOSION` (used by disasters) |
| Crime Wave | Alarm / siren | New sound needed (or no sound, just news) |

### 9.5 News Messages

All vice events generate news items using the existing `NewsType::Accident` category (same as disasters). This ensures:
- Players see vice news in their normal news flow
- News message settings (full/summary/off) apply
- News history is maintained

| Event | News Style | String Example |
|---|---|---|
| Petty Vandalism | `NewsStyle::Thin` | "Vandals damage {station_name} in {town_name}" |
| Vehicle Sabotage | `NewsStyle::Thin` | "Vehicle sabotaged near {town_name}" |
| Arson | `NewsStyle::Small` | "Arson destroys building in {town_name}" |
| Riot | `NewsStyle::Large` | "Riots break out in {town_name}! {n} buildings destroyed" |
| Crime Wave | `NewsStyle::Large` | "Crime wave grips {town_name}! Authorities warn of ongoing disruption" |

### 9.6 Vice Level Change Notifications

Beyond event-specific news, players receive **proactive notifications when a town's vice level crosses key thresholds**. This gives players warning before events start happening, enabling prevention rather than only reaction.

#### Threshold Notifications

Vice level is recalculated monthly. When it crosses a threshold boundary (in either direction), a news message is generated:

| Vice Level | Threshold Name | Direction | News Style | Message |
|---|---|---|---|---|
| 20 | Concern | Rising | `NewsStyle::Thin` | "Petty crime rising in {town_name}" |
| 20 | Concern | Falling | `NewsStyle::Thin` | "Crime declining in {town_name}" |
| 50 | Warning | Rising | `NewsStyle::Small` | "Crime rate surges in {town_name} — authorities urge action" |
| 50 | Warning | Falling | `NewsStyle::Small` | "Crime rate falling in {town_name} — improvements noted" |
| 75 | Critical | Rising | `NewsStyle::Large` | "Crime crisis in {town_name}! Residents fear for safety" |
| 75 | Critical | Falling | `NewsStyle::Large` | "{town_name} crime crisis easing — situation improving" |

**Implementation**: The town stores `vice_level_prev` (previous month's level). On monthly recalculation, if `vice_level` and `vice_level_prev` are on different sides of a threshold (20, 50, 75), the corresponding notification fires. This prevents spam — only transitions trigger messages.

#### Town Window Vice Display

The Town View window shows the current vice status at all times (when the feature is enabled):

| Vice Level | Label | Colour |
|---|---|---|
| 0-10 | "Crime: Peaceful" | Green |
| 11-20 | "Crime: Low" | Light Green |
| 21-35 | "Crime: Moderate" | Yellow |
| 36-50 | "Crime: Elevated" | Orange |
| 51-75 | "Crime: High" | Red |
| 76-100 | "Crime: Critical" | Flashing Red |

A tooltip on hover shows: "Vice level: {vice_level}/100 — Service coverage: {pct}%, Authority rating: {rating}"

This gives the player full transparency into the formula inputs, so they know exactly what levers to pull.

### 9.7 Policing Investment — Cost vs. Vice Reduction

Players can invest in policing through the town authority window. Rather than a single flat action, policing uses a **tiered investment system** where spending more money yields diminishing returns — reflecting the real-world economics of crime prevention.

#### Fund Policing Action

Accessed via Town Authority window > "Fund Policing". The player selects an investment tier:

| Investment Tier | Cost (£) | Vice Reduction | Duration | Cost per Vice Point | Cost per Vice Point per Month | Efficiency |
|---|---|---|---|---|---|---|
| Basic Patrol | 25,000 | -10 | 3 months | £2,500 | £833 | Best value |
| Enhanced Patrol | 50,000 | -20 | 4 months | £2,500 | £625 | Good value |
| Police Station | 100,000 | -30 | 6 months | £3,333 | £556 | Best per-month |
| Security Force | 200,000 | -45 | 6 months | £4,444 | £741 | Premium |
| Martial Law | 500,000 | -70 | 6 months | £7,143 | £1,190 | Emergency only |

#### Annual Cost Projection by Town Population

This table shows the recommended policing spend to keep vice manageable, compared to expected revenue from the town:

| Population | Base Vice | Vice with 50% Svc | Recommended Tier | Annual Cost | Expected Annual Revenue | Policing as % of Revenue |
|---|---|---|---|---|---|---|
| 1,000 | 16 | 0 | None needed | £0 | £30,000 | 0% |
| 3,000 | 37 | 14 | None needed | £0 | £90,000 | 0% |
| 5,000 | 50 | 27 | Basic Patrol | £50,000 | £150,000 | 33% |
| 8,000 | 61 | 38 | Enhanced Patrol | £100,000 | £240,000 | 42% |
| 10,000 | 66 | 43 | Enhanced Patrol | £100,000 | £300,000 | 33% |
| 15,000 | 75 | 52 | Police Station | £200,000 | £450,000 | 44% |
| 20,000 | 80 | 57 | Police Station | £200,000 | £600,000 | 33% |
| 30,000 | 85 | 62 | Security Force | £400,000 | £900,000 | 44% |
| 50,000 | 90 | 67 | Security Force | £400,000 | £1,500,000 | 27% |
| 100,000 | 95 | 72 | Martial Law | £1,000,000 | £3,000,000 | 33% |

#### Combined Mitigation: Service + Policing

The most cost-effective strategy combines good transport service with targeted policing. This table shows final vice levels after applying both:

| Population | No Mitigation | 80% Service Only | Police Station Only | 80% Service + Enhanced Patrol | 80% Service + Police Station |
|---|---|---|---|---|---|
| 5,000 | 50 | 3 | 20 | 0 | 0 |
| 10,000 | 66 | 19 | 36 | 0 | 0 |
| 15,000 | 75 | 28 | 45 | 8 | 0 |
| 20,000 | 80 | 33 | 50 | 13 | 3 |
| 30,000 | 85 | 38 | 55 | 18 | 8 |
| 50,000 | 90 | 43 | 60 | 23 | 13 |
| 100,000 | 95 | 48 | 65 | 28 | 18 |

**Key insight**: Good transport service (free if you already run routes) combined with mid-tier policing keeps even large cities well under control. Martial Law is only needed if service coverage is poor — it's a fallback, not the intended solution.

#### Integer Math for Policing

```
// policing_reduction is set when player funds policing (10, 20, 30, 45, or 70)
// policing_months is the countdown timer

if (policing_months > 0) {
    final_vice = max(0, final_vice - policing_reduction)
}
```

Multiple tiers do NOT stack — funding a new tier replaces the current one. This prevents exploits where rich players stack reductions to zero.

---

## Appendix A: New Data Fields

### Town struct additions

| Field | Type | Default | Purpose |
|---|---|---|---|
| `vice_level` | `uint8_t` | 0 | Cached vice level, recalculated monthly |
| `vice_level_prev` | `uint8_t` | 0 | Previous month's vice level (for threshold notifications) |
| `vice_cooldown` | `uint8_t` | 0 | Months until next vice event can trigger |
| `crime_wave_months` | `uint8_t` | 0 | Remaining months of active Crime Wave |
| `policing_months` | `uint8_t` | 0 | Remaining months of Fund Policing effect |
| `policing_reduction` | `uint8_t` | 0 | Vice reduction from current policing tier (10/20/30/45/70) |

### Savegame compatibility

These fields require a new savegame version bump. For backward compatibility with older savegames, all fields default to 0 (no vice activity), which means loading an old save into a version with City Vice starts with a clean slate. This is the standard approach used by other feature additions.

---

## Appendix B: Integration Points in Existing Code

| File | Function/Location | Change Required |
|---|---|---|
| `town.h` | `struct Town` | Add 4 new `uint8_t` fields |
| `town_cmd.cpp` | `_economy_towns_monthly` callback | Add vice level calculation and event trigger check |
| `town_cmd.cpp` | `_town_action_proc[]` | Add `TownActionFundPolicing` |
| `town_cmd.cpp` | `GetTownActionCost()` | Add cost for policing action |
| `town.h` | `enum TownAction` | Add `FundPolicing` before `End` |
| `disaster_vehicle.cpp` | (none) | No changes -- vice uses a separate system |
| `economy_type.h` | `ExpensesType` | No changes -- uses existing `EXPENSES_PROPERTY` |
| `saveload/town_sl.cpp` | Save/load descriptors | Add new fields |
| `script/api/` | Script API | Add new event classes |
| `lang/english.txt` | Strings | Add ~20 new string IDs |
| `table/settings/economy_settings.ini` | Settings | Add 4 new settings |

---

## Appendix C: Full Integer Math Reference

All formulas in this document use integer arithmetic only. Here is the complete set:

```
// 1. Base vice level (0-99)
base_vice = (population * 100) / (population + 5000)

// 2. Service modifier (0-60)
pct_transported = GetPercentTransported(passengers)  // 0-255
service_modifier = (pct_transported * 60) / 256

// 3. Rating modifier (-20 to +20)
best_rating = max(all company ratings in town)  // -1000 to +1000
rating_modifier = -(best_rating * 20) / 1000

// 4. Final vice (0-100)
adjusted_vice = max(0, base_vice - service_modifier)
final_vice = clamp(adjusted_vice + rating_modifier, 0, 100)

// 5. Monthly event probability check
effective_vice = final_vice * vice_intensity_setting  // 0-2000
if severity == Harsh: effective_vice = effective_vice * 3 / 2
roll = RandomRange(1000)
event_occurs = (roll < effective_vice)

// 6. Event type selection
// Weights per tier (see Section 3.3 table)
total_weight = sum(weights for population tier)
type_roll = RandomRange(total_weight)
// Walk through cumulative weights to select type

// 7. Riot building count
riot_buildings = 1 + RandomRange(clamp(population / 20000, 0, 2))

// 8. Fund Policing effect
// While policing_months > 0: final_vice = max(0, final_vice - 30)
```

---

## Appendix D: Playtesting Checklist

Before implementation, the following scenarios should be validated in testing:

- [ ] Town at exactly 999 population: zero vice events
- [ ] Town at exactly 1000 population: vice events possible but very rare
- [ ] Well-serviced city at 20,000: fewer than 3 events per year on Normal difficulty
- [ ] Neglected city at 20,000: 5-8 events per year on Normal difficulty
- [ ] Metropolis at 100,000, well-serviced: manageable (~4-6 events/year), majority are minor
- [ ] Fund Policing effect: measurably reduces events
- [ ] Crime Wave does not cascade into growth stagnation
- [ ] Riot does not cascade (cooldown prevents rapid-fire events)
- [ ] AI company with bus routes in a high-vice city: not disproportionately punished
- [ ] Multiplayer: vice affects all companies in a town fairly
- [ ] Settings: intensity 0 produces zero events; intensity 20 produces many events
- [ ] Severity Mild: only Vandalism and Sabotage occur
- [ ] Save/Load: vice state persists correctly across save/load cycle
- [ ] No floating point: all calculations verified as integer-only
