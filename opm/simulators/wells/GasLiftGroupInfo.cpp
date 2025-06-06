/*
  Copyright 2021 Equinor ASA.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>
#include <opm/simulators/wells/GasLiftGroupInfo.hpp>

#include <opm/input/eclipse/Schedule/GasLiftOpt.hpp>
#include <opm/input/eclipse/Schedule/Schedule.hpp>
#include <opm/input/eclipse/Schedule/Well/Well.hpp>

#include <opm/simulators/wells/WellState.hpp>

#include <fmt/format.h>

#include <stdexcept>

namespace Opm {

template<class Scalar>
GasLiftGroupInfo<Scalar>::
GasLiftGroupInfo(GLiftEclWells& ecl_wells,
                 const Schedule& schedule,
                 const SummaryState& summary_state,
                 const int report_step_idx,
                 const int iteration_idx,
                 const PhaseUsage& phase_usage,
                 DeferredLogger& deferred_logger,
                 WellState<Scalar>& well_state,
                 const GroupState<Scalar>& group_state,
                 const Communication& comm,
                 bool glift_debug)
    : GasLiftCommon<Scalar>(well_state, group_state, deferred_logger, comm, glift_debug)
    , ecl_wells_{ecl_wells}
    , schedule_{schedule}
    , summary_state_{summary_state}
    , report_step_idx_{report_step_idx}
    , iteration_idx_{iteration_idx}
    , phase_usage_{phase_usage}
    , glo_{schedule_.glo(report_step_idx_)}
{}

/****************************************
 * Public methods in alphabetical order
 ****************************************/

template<class Scalar>
Scalar GasLiftGroupInfo<Scalar>::
alqRate(const std::string& group_name)
{
    auto& group_rate = this->group_rate_map_.at(group_name);
    return group_rate.alq();
}

template<class Scalar>
int GasLiftGroupInfo<Scalar>::
getGroupIdx(const std::string& group_name)
{
    return this->group_idx_.at(group_name);
}

template<class Scalar>
Scalar GasLiftGroupInfo<Scalar>::
gasRate(const std::string& group_name) const
{
    auto& group_rate = this->group_rate_map_.at(group_name);
    return group_rate.gasRate();
}

template<class Scalar>
Scalar GasLiftGroupInfo<Scalar>::
gasPotential(const std::string& group_name) const
{
    auto& group_rate = this->group_rate_map_.at(group_name);
    return group_rate.gasPotential();
}

template<class Scalar>
Scalar GasLiftGroupInfo<Scalar>::
waterPotential(const std::string& group_name) const
{
    auto& group_rate = this->group_rate_map_.at(group_name);
    return group_rate.waterPotential();
}

template<class Scalar>
Scalar GasLiftGroupInfo<Scalar>::
oilPotential(const std::string& group_name) const
{
    auto& group_rate = this->group_rate_map_.at(group_name);
    return group_rate.oilPotential();
}

template<class Scalar>
std::optional<Scalar>
GasLiftGroupInfo<Scalar>::
gasTarget(const std::string& group_name) const
{
    auto& group_rate = this->group_rate_map_.at(group_name);
    return group_rate.gasTarget();
}

template<class Scalar>
Scalar GasLiftGroupInfo<Scalar>::
getRate(Rate rate_type, const std::string& group_name) const
{
    switch (rate_type) {
    case Rate::oil:
        return oilRate(group_name);
    case Rate::gas:
        return gasRate(group_name);
    case Rate::water:
        return waterRate(group_name);
    case Rate::liquid:
        return oilRate(group_name) + waterRate(group_name);
    default:
        // Need this to avoid compiler warning : control reaches end of non-void function
        throw std::runtime_error("This should not happen");
    }
}

template<class Scalar>
Scalar GasLiftGroupInfo<Scalar>::
getPotential(Rate rate_type, const std::string& group_name) const
{
    switch (rate_type) {
    case Rate::oil:
        return oilPotential(group_name);
    case Rate::gas:
        return gasPotential(group_name);
    case Rate::water:
        return waterPotential(group_name);
    case Rate::liquid:
        return oilPotential(group_name) + waterPotential(group_name);
    default:
        // Need this to avoid compiler warning : control reaches end of non-void function
        throw std::runtime_error("This should not happen");
    }
}

template<class Scalar>
std::tuple<Scalar, Scalar, Scalar, Scalar>
GasLiftGroupInfo<Scalar>::
getRates(const int group_idx) const
{
    const auto& group_name = groupIdxToName(group_idx);
    auto& rates = this->group_rate_map_.at(group_name);
    return std::make_tuple(rates.oilRate(), rates.gasRate(), rates.waterRate(), rates.alq());
}

template<class Scalar>
std::optional<Scalar>
GasLiftGroupInfo<Scalar>::
getTarget(Rate rate_type, const std::string& group_name) const
{
    switch (rate_type) {
    case Rate::oil:
        return oilTarget(group_name);
    case Rate::gas:
        return gasTarget(group_name);
    case Rate::water:
        return waterTarget(group_name);
    case Rate::liquid:
        return liquidTarget(group_name);
    default:
        // Need this to avoid compiler warning : control reaches end of non-void function
        throw std::runtime_error("This should not happen");
    }
}

template<class Scalar>
std::vector<std::pair<std::string,Scalar>>&
GasLiftGroupInfo<Scalar>::
getWellGroups(const std::string& well_name)
{
    assert(this->well_group_map_.count(well_name) == 1);
    return this->well_group_map_[well_name];
}

template<class Scalar>
const std::string&
GasLiftGroupInfo<Scalar>::
groupIdxToName(int group_idx) const
{
    const std::string* group_name = nullptr;
    // TODO:  An alternative to the below loop is to set up a reverse map from idx ->
    //   string, then we could in theory do faster lookup here..
    for (const auto& [key, value] : this->group_idx_) {
        if (value == group_idx) {
            // NOTE: it is assumed that the mapping from name->idx is one-to-one
            //   so there can only be one idx with a given group name.
            group_name = &key;
            break;
        }
    }
    // the caller is responsible for providing a valid idx, so group_name
    //   cannot be nullptr here..
    assert(group_name);
    return *group_name;
}

template<class Scalar>
bool GasLiftGroupInfo<Scalar>::
hasAnyTarget(const std::string& group_name) const
{
    return oilTarget(group_name)   || gasTarget(group_name)
        || waterTarget(group_name) || liquidTarget(group_name);
}

template<class Scalar>
bool GasLiftGroupInfo<Scalar>::
hasWell(const std::string& well_name)
{
    return this->well_group_map_.count(well_name) == 1;
}

template<class Scalar>
void GasLiftGroupInfo<Scalar>::
initialize()
{
    const auto& group = this->schedule_.getGroup("FIELD", this->report_step_idx_);
    initializeGroupRatesRecursive_(group);
    std::vector<std::string> group_names;
    std::vector<Scalar> group_efficiency;
    initializeWell2GroupMapRecursive_(
        group, group_names, group_efficiency, /*current efficiency=*/1.0);
}

template<class Scalar>
std::optional<Scalar>
GasLiftGroupInfo<Scalar>::
liquidTarget(const std::string& group_name) const
{
    auto& group_rate = this->group_rate_map_.at(group_name);
    return group_rate.liquidTarget();
}

template<class Scalar>
std::optional<Scalar>
GasLiftGroupInfo<Scalar>::
maxAlq(const std::string& group_name)
{
    auto& group_rate = this->group_rate_map_.at(group_name);
    return group_rate.maxAlq();
}

template<class Scalar>
std::optional<Scalar>
GasLiftGroupInfo<Scalar>::
maxTotalGasRate(const std::string& group_name)
{
    auto& group_rate = this->group_rate_map_.at(group_name);
    return group_rate.maxTotalGasRate();
}

template<class Scalar>
Scalar GasLiftGroupInfo<Scalar>::
oilRate(const std::string& group_name) const
{
    auto& group_rate = this->group_rate_map_.at(group_name);
    return group_rate.oilRate();
}

template<class Scalar>
std::optional<Scalar>
GasLiftGroupInfo<Scalar>::
oilTarget(const std::string& group_name) const
{
    auto& group_rate = this->group_rate_map_.at(group_name);
    return group_rate.oilTarget();
}

template<class Scalar>
const std::string
GasLiftGroupInfo<Scalar>::
rateToString(Rate rate) {
    switch (rate) {
    case Rate::oil:
        return "oil";
    case Rate::gas:
        return "gas";
    case Rate::water:
        return "water";
    case Rate::liquid:
        return "liquid";
    default:
        throw std::runtime_error("This should not happen");
    }
}

template<class Scalar>
Scalar GasLiftGroupInfo<Scalar>::
waterRate(const std::string& group_name) const
{
    auto& group_rate = this->group_rate_map_.at(group_name);
    return group_rate.waterRate();
}

template<class Scalar>
std::optional<Scalar>
GasLiftGroupInfo<Scalar>::
waterTarget(const std::string& group_name) const
{
    auto& group_rate = this->group_rate_map_.at(group_name);
    return group_rate.waterTarget();
}

template<class Scalar>
void GasLiftGroupInfo<Scalar>::
update(const std::string& group_name,
        Scalar delta_oil,
        Scalar delta_gas,
        Scalar delta_water,
        Scalar delta_alq)
{
    auto& group_rate = this->group_rate_map_.at(group_name);
    group_rate.update(delta_oil, delta_gas, delta_water, delta_alq);
}

template<class Scalar>
void GasLiftGroupInfo<Scalar>::
updateRate(int idx,
           Scalar oil_rate,
           Scalar gas_rate,
           Scalar water_rate,
           Scalar alq)
{
    const auto& group_name = groupIdxToName(idx);
    auto& rates = this->group_rate_map_.at(group_name);
    rates.assign(oil_rate, gas_rate, water_rate, alq);
}

/****************************************
 * Protected methods in alphabetical order
 ****************************************/

template<class Scalar>
bool GasLiftGroupInfo<Scalar>::
checkDoGasLiftOptimization_(const std::string& well_name)
{
    auto itr = this->ecl_wells_.find(well_name);
    if (itr == this->ecl_wells_.end()) {
        // well_name is not present in the well_model's well container
        //displayDebugMessage_("Could not find well in ecl_wells. Skipping.", well_name);
        return false;
    }

    if (this->well_state_.well(well_name).alq_state.oscillation()) {
        displayDebugMessage_(
             "further optimization skipped due to oscillation in ALQ", well_name);
        return false;
    }

    const Well *well = (itr->second).first;
    //assert(well); // Should never be nullptr
    if (well->isInjector()) {
        displayDebugMessage_("Injector well. Skipping", well_name);
        return false;
    }
    if (this->optimize_only_thp_wells_) {
        const int well_index = (itr->second).second;
        const auto& ws = this->well_state_.well(well_index);
        const Well::ProducerCMode& control_mode = ws.production_cmode;
        if (control_mode != Well::ProducerCMode::THP ) {
            displayDebugMessage_("Not THP control. Skipping.", well_name);
            return false;
        }
    }
    if (!checkNewtonIterationIdxOk_(well_name)) {
        return false;
    }
    if (!this->glo_.has_well(well_name)) {
        displayDebugMessage_(
             "Gas Lift not activated: WLIFTOPT is probably missing. Skipping.", well_name);
        return false;
    }
    auto increment = this->glo_.gaslift_increment();
    // NOTE: According to the manual: LIFTOPT, item 1, :
    //   "Increment size for lift gas injection rate. Lift gas is
    //   allocated to individual wells in whole numbers of the increment
    //   size.  If gas lift optimization is no longer required, it can be
    //   turned off by entering a zero or negative number."
    if (increment <= 0) {
        if (this->debug) {
            const std::string msg = fmt::format(
                "Gas Lift switched off in LIFTOPT item 1 due to non-positive "
                "value: {}", increment);
                displayDebugMessage_(msg, well_name);
        }
        return false;
    }
    else {
        return true;
    }
}

template<class Scalar>
bool GasLiftGroupInfo<Scalar>::
checkNewtonIterationIdxOk_(const std::string& well_name)
{
    if (this->glo_.all_newton()) {
        const int nupcol = this->schedule_[this->report_step_idx_].nupcol();
        if (this->debug) {
            const std::string msg = fmt::format(
                "LIFTOPT item4 == YES, it = {}, nupcol = {} -->  GLIFT optimize = {}",
                this->iteration_idx_,
                nupcol,
                ((this->iteration_idx_ < nupcol) ? "TRUE" : "FALSE"));
            displayDebugMessage_(msg, well_name);
        }
        return this->iteration_idx_ < nupcol;
    }
    else {
        if (this->debug) {
            const std::string msg = fmt::format(
                    "LIFTOPT item4 == NO, it = {} --> GLIFT optimize = {}",
                    this->iteration_idx_,
                    ((this->iteration_idx_ == 1) ? "TRUE" : "FALSE"));
            displayDebugMessage_(msg, well_name);
        }
        return this->iteration_idx_ == 1;
    }
}

// This is called by each rank, but the value of "well_name" should be unique
// across ranks
template<class Scalar>
void GasLiftGroupInfo<Scalar>::
debugDisplayWellContribution_(const std::string& gr_name,
                              const std::string& well_name,
                              Scalar eff_factor,
                              Scalar well_oil_rate,
                              Scalar well_gas_rate,
                              Scalar well_water_rate,
                              Scalar well_alq,
                              Scalar oil_rate,
                              Scalar gas_rate,
                              Scalar water_rate,
                              Scalar alq) const
{
    const std::string msg = fmt::format("Group rate for {} : Well {} : "
        "eff_factor = {}, oil_rate = {}, gas_rate = {}, water_rate = {}, "
        "alq = {}, New group rates:  oil = {}, gas = {}, water = {}, alq = {}",
        gr_name, well_name, eff_factor, well_oil_rate, well_gas_rate,
        well_water_rate, well_alq, oil_rate, gas_rate, water_rate, alq);
    displayDebugMessage_(msg);
}

template<class Scalar>
void GasLiftGroupInfo<Scalar>::
debugDisplayUpdatedGroupRates(const std::string& name,
                              Scalar oil_rate,
                              Scalar gas_rate,
                              Scalar water_rate,
                              Scalar alq) const
{
    const std::string msg = fmt::format("Updated group info for {} : "
        "oil_rate = {}, gas_rate = {}, water_rate = {}, alq = {}",
        name, oil_rate, gas_rate, water_rate, alq);
    this->displayDebugMessageOnRank0_(msg);
}

template<class Scalar>
void GasLiftGroupInfo<Scalar>::
debugEndInitializeGroup(const std::string& name) const
{
    const std::string msg = fmt::format("Finished with group {} ...", name);
    this->displayDebugMessageOnRank0_(msg);
}

template<class Scalar>
void GasLiftGroupInfo<Scalar>::
debugStartInitializeGroup(const std::string& name) const
{
    const std::string msg = fmt::format("Initializing group {} ...", name);
    this->displayDebugMessageOnRank0_(msg);
}

template<class Scalar>
void GasLiftGroupInfo<Scalar>::
displayDebugMessage_(const std::string& msg) const
{
    if (this->debug) {
        const std::string message = fmt::format("Init group info : {}", msg);
        this->logMessage_(/*prefix=*/"GLIFT", message);
    }
}

template<class Scalar>
void GasLiftGroupInfo<Scalar>::
displayDebugMessage_(const std::string& msg, const std::string& well_name)
{
    if (this->debug) {
        const std::string message = fmt::format("Well {} : {}", well_name, msg);
        displayDebugMessage_(message);
    }
}

template<class Scalar>
std::tuple<Scalar, Scalar, Scalar, Scalar, Scalar, Scalar>
GasLiftGroupInfo<Scalar>::
getProducerWellRates_(const Well* well, int well_index)
{
    const auto& pu = this->phase_usage_;
    const auto& ws= this->well_state_.well(well_index);
    const auto& wrate = ws.well_potentials;

    const auto oil_pot = pu.phase_used[Oil]
        ? wrate[pu.phase_pos[Oil]]
        : 0.0;

    const auto gas_pot = pu.phase_used[Gas]
        ? wrate[pu.phase_pos[Gas]]
        : 0.0;

    const auto water_pot = pu.phase_used[Water]
        ? wrate[pu.phase_pos[Water]]
        : 0.0;

    const auto controls = well->productionControls(this->summary_state_);
    Scalar oil_rate = oil_pot;
    Scalar water_rate = water_pot;
    Scalar gas_rate = gas_pot;

    if (controls.hasControl(Well::ProducerCMode::ORAT) && oil_rate > static_cast<Scalar>(controls.oil_rate)) {
        water_rate *= (static_cast<Scalar>(controls.oil_rate) / oil_rate); 
        oil_rate = static_cast<Scalar>(controls.oil_rate);
    }
    
    if (controls.hasControl(Well::ProducerCMode::GRAT)) {
        gas_rate = std::min(static_cast<Scalar>(controls.gas_rate), gas_rate);
    }
    
    if (controls.hasControl(Well::ProducerCMode::WRAT) && water_rate > static_cast<Scalar>(controls.water_rate)) {
        oil_rate *= (static_cast<Scalar>(controls.water_rate) / water_rate); 
        water_rate = static_cast<Scalar>(controls.water_rate);
    }
    if (controls.hasControl(Well::ProducerCMode::LRAT)) {
        Scalar liquid_rate = oil_rate + water_rate;
        Scalar liquid_rate_lim = static_cast<Scalar>(controls.liquid_rate);
        if (liquid_rate > liquid_rate_lim) {
            water_rate = water_rate / liquid_rate * liquid_rate_lim;
            oil_rate = oil_rate / liquid_rate * liquid_rate_lim;
        }
    }

    return {oil_rate, gas_rate, water_rate, oil_pot, gas_pot, water_pot};
}

template<class Scalar>
std::tuple<Scalar, Scalar, Scalar, Scalar, Scalar, Scalar, Scalar>
GasLiftGroupInfo<Scalar>::
initializeGroupRatesRecursive_(const Group& group)
{
    std::array<Scalar,7> rates{};
    if (this->debug) debugStartInitializeGroup(group.name());
    auto& [oil_rate, water_rate, gas_rate, oil_potential, water_potential, gas_potential, alq] = rates;
    if (group.wellgroup()) {
        for (const std::string& well_name : group.wells()) {
            // NOTE: we cannot simply use:
            //
            //  const auto &well =
            //    this->schedule_.getWell(well_name, this->report_step_idx_);
            //
            // since the well may not be active (present in the well container)
            auto itr = this->ecl_wells_.find(well_name);
            if (itr != this->ecl_wells_.end()) {
                const Well *well = (itr->second).first;
                assert(well); // Should never be nullptr
                const int index = (itr->second).second;
                if (well->isProducer()) {
                    auto [sw_oil_rate, sw_gas_rate, sw_water_rate, sw_oil_pot, sw_gas_pot, sw_water_pot] = getProducerWellRates_(well, index);
                    auto sw_alq = this->well_state_.well(well_name).alq_state.get();
                    const Scalar factor = well->getEfficiencyFactor() *
                                          this->well_state_.getGlobalEfficiencyScalingFactor(well_name);
                    oil_rate += (factor * sw_oil_rate);
                    gas_rate += (factor * sw_gas_rate);
                    water_rate += (factor * sw_water_rate);
                    oil_potential += (factor * sw_oil_pot);
                    gas_potential += (factor * sw_gas_pot);
                    water_potential += (factor * sw_water_pot);

                    alq += (factor * sw_alq);
                    if (this->debug) {
                        debugDisplayWellContribution_(
                            group.name(), well_name, factor,
                            sw_oil_pot, sw_gas_pot, sw_water_pot, sw_alq,
                            oil_rate, gas_rate, water_rate, alq
                        );
                    }
                }
            }
        }
        this->comm_.sum(rates.data(), rates.size());
    }
    else {
        for (const std::string& group_name : group.groups()) {
            if (!this->schedule_.back().groups.has(group_name))
                continue;
            const Group& sub_group = this->schedule_.getGroup(
                group_name, this->report_step_idx_);
            auto [sg_oil_rate, sg_gas_rate, sg_water_rate,
                  sg_oil_pot, sg_gas_pot, sg_water_pot, sg_alq]
                = initializeGroupRatesRecursive_(sub_group);
            const auto gefac = sub_group.getGroupEfficiencyFactor();
            oil_rate += (gefac * sg_oil_rate);
            gas_rate += (gefac * sg_gas_rate);
            water_rate += (gefac * sg_water_rate);
            oil_potential += (gefac * sg_oil_pot);
            gas_potential += (gefac * sg_gas_pot);
            water_potential += (gefac * sg_water_pot);
            alq += (gefac * sg_alq);
        }
    }
    if (this->debug) debugEndInitializeGroup(group.name());
    std::optional<Scalar> oil_target, gas_target, water_target, liquid_target, max_total_gas, max_alq;
    const auto controls = group.productionControls(this->summary_state_);
    if (group.has_control(Group::ProductionCMode::LRAT)) {
        liquid_target = controls.liquid_target;
    }
    if (group.has_control(Group::ProductionCMode::ORAT)) {
        oil_target = controls.oil_target;
    }
    if (group.has_control(Group::ProductionCMode::GRAT)) {
        gas_target = controls.gas_target;
    }
    if (group.has_control(Group::ProductionCMode::WRAT)) {
        water_target = controls.water_target;
    }
    if (this->glo_.has_group(group.name())) {
        const auto& gl_group = this->glo_.group(group.name());
        max_alq = gl_group.max_lift_gas();
        max_total_gas = gl_group.max_total_gas();
    }
    if (oil_target || liquid_target || water_target || gas_target || max_total_gas || max_alq) {
        updateGroupIdxMap_(group.name());
        this->group_rate_map_.try_emplace(group.name(),
            oil_rate, gas_rate, water_rate, alq,
            oil_potential, gas_potential, water_potential,
            oil_target, gas_target, water_target, liquid_target, max_total_gas, max_alq);
        if (this->debug) {
            debugDisplayUpdatedGroupRates(
                group.name(), oil_rate, gas_rate, water_rate, alq);
        }

        if (oil_target && oil_rate > *oil_target)  {
            water_rate *= (*oil_target/oil_rate);
            oil_rate = *oil_target;
        }
        if (gas_target && gas_rate > *gas_target)
            gas_rate = *gas_target;
        if (water_target && water_rate > *water_target) {
            oil_rate *= (*water_target/water_rate); 
            water_rate = *water_target;
        }
        if (liquid_target) {
            Scalar liquid_rate = oil_rate + water_rate;
            Scalar liquid_rate_limited = *liquid_target;
            if (liquid_rate > liquid_rate_limited) {
                oil_rate = oil_rate / liquid_rate * liquid_rate_limited;
                water_rate = water_rate / liquid_rate * liquid_rate_limited;
            }
        }
    }
    return std::make_tuple(oil_rate, gas_rate, water_rate, oil_potential, gas_potential, water_potential, alq);
}

template<class Scalar>
void GasLiftGroupInfo<Scalar>::
initializeWell2GroupMapRecursive_(const Group& group,
                                  std::vector<std::string>& group_names,
                                  std::vector<Scalar>& group_efficiency,
                                  Scalar cur_efficiency)
{
    Scalar gfac = group.getGroupEfficiencyFactor();
    cur_efficiency = gfac * cur_efficiency;
    std::transform(group_efficiency.begin(), group_efficiency.end(),
                   group_efficiency.begin(),
                   [gfac](const auto& item) { return item * gfac; });
    if (this->group_rate_map_.count(group.name()) == 1) {
        // extract the subset of groups that has limits or targets that can affect
        //   gas lift optimization.
        group_names.push_back(group.name());
        group_efficiency.push_back(gfac);
    }
    if (group.wellgroup()) {
        for (const std::string& well_name : group.wells()) {
            // TODO: can the same well be memember of two different groups
            //  (on the same recursion level) ?
            assert(this->well_group_map_.count(well_name) == 0);
            bool checkDoGasLift = checkDoGasLiftOptimization_(well_name);
            checkDoGasLift = this->comm_.max(checkDoGasLift);
            if (checkDoGasLift) {
                const auto &well = this->schedule_.getWell(
                    well_name, this->report_step_idx_);
                const Scalar wfac = well.getEfficiencyFactor() *
                                    this->well_state_.getGlobalEfficiencyScalingFactor(well_name);
                auto [itr, success] = this->well_group_map_.insert(
                      {well_name, /*empty vector*/ {}});
                assert(success);
                auto& vec = itr->second;
                assert(group_names.size() == group_efficiency.size());
                auto iter2 = group_efficiency.begin();
                for (auto iter1 = group_names.begin();
                     iter1 != group_names.end(); ++iter1)
                {
                    Scalar efficiency = (*iter2) * wfac;
                    vec.emplace_back(/*group_name=*/*iter1, efficiency);
                    ++iter2;
                }
            }
        }
    }
    else {
        for (const std::string& group_name : group.groups()) {
            if (!this->schedule_.back().groups.has(group_name))
                continue;
            const Group& sub_group = this->schedule_.getGroup(
                group_name, this->report_step_idx_);
            initializeWell2GroupMapRecursive_(
                sub_group, group_names, group_efficiency, cur_efficiency);
        }
    }
    if (this->group_rate_map_.count(group.name()) == 1) {
        group_names.pop_back();
        group_efficiency.pop_back();
    }
}

// TODO: It would be more efficient if the group idx map was build once
//  per time step (or better: once per report step) and saved e.g. in
//  the well state object, instead of rebuilding here for each of
//  NUPCOL well iteration for each time step.
template<class Scalar>
void GasLiftGroupInfo<Scalar>::
updateGroupIdxMap_(const std::string& group_name)
{
    if (this->group_idx_.count(group_name) == 0) {
        //auto [itr, success] =
        this->group_idx_.try_emplace(group_name, this->next_group_idx_);
        this->next_group_idx_++;
    }
}

template class GasLiftGroupInfo<double>;

#if FLOW_INSTANTIATE_FLOAT
template class GasLiftGroupInfo<float>;
#endif

} // namespace Opm
