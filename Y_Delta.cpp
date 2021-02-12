#include <memory>
#include <string>
#include <cstdio>

#include "halo_particles.hpp"

namespace Y_Delta
{
    constexpr const size_t PartType = 0; // gas

    typedef GrpFields<IllustrisFields::GroupPos,
                      IllustrisFields::GroupMass,
                      IllustrisFields::Group_R_Crit200> GrpF;
    typedef PrtFields<IllustrisFields::Coordinates,
                      IllustrisFields::Masses,
                      IllustrisFields::InternalEnergy,
                      IllustrisFields::ElectronAbundance> PrtF;

    typedef double grp_Y_t;
    typedef double grp_M_t;
} // namespace Y_Delta

struct Y_Delta_Callback :
    virtual public Callback,
    public CallbackUtils::chunk_fmt::Multi,
    public CallbackUtils::illustris::Conventional
                < Y_Delta::PartType >,
    public CallbackUtils::select::GrpMassLowCutoff
                < Y_Delta::GrpF::idx<IllustrisFields::GroupMass> >,
    public CallbackUtils::select::PrtAll,
    public CallbackUtils::radius::Simple
                < Y_Delta::GrpF::idx<IllustrisFields::Group_R_Crit200> >,
    public CallbackUtils::actions::StoreHomogeneous
                <Y_Delta::grp_Y_t>,
    public CallbackUtils::actions::StoreGrpProperties
                <Y_Delta::grp_M_t>
{// {{{
    Y_Delta_Callback () :
        CallbackUtils::chunk_fmt::Multi
                { fgrp, grp_max_idx, fprt, prt_max_idx },
        CallbackUtils::select::GrpMassLowCutoff
            < Y_Delta::GrpF::idx<IllustrisFields::GroupMass> >
                { Mmin },
        CallbackUtils::radius::Simple
            < Y_Delta::GrpF::idx<IllustrisFields::Group_R_Crit200> >
                { Rscale },
        CallbackUtils::actions::StoreHomogeneous
            <Y_Delta::grp_Y_t>
                { &grp_Y },
        CallbackUtils::actions::StoreGrpProperties
            <Y_Delta::grp_M_t>
                { &grp_M }
    { }

    // data (public so user can do something with them once they are assembled)
    std::vector<Y_Delta::grp_Y_t> grp_M;
    std::vector<Y_Delta::grp_M_t> grp_Y;

private :
    // for calculation of electron pressure
    static constexpr const float gamma = 5.0F/3.0F;
    static constexpr const float XH    = 0.76F;

    // group mass cutoff
    static constexpr const float Mmin = 1e3F;

    // radial cutoff
    static constexpr const float Rscale = 1.0F;

    // files
    #define ROOT "/tigress/lthiele/Illustris_300-1_TNG/output/"
    static constexpr const char fgrp[]        = ROOT"groups_099/fof_subhalo_tab_099.%d.hdf5";
    static constexpr const size_t grp_max_idx = 599;
    static constexpr const char fprt[]        = ROOT"snapdir_099/snap_099.%d.hdf5";
    static constexpr const size_t prt_max_idx = 599;
    #undef ROOT

    // The next three functions are required -- they define the functionality of this class
    
    // computation of Compton-Y for a single gas particle
    Y_Delta::grp_Y_t prt_reduce (size_t grp_idx,
                                 void **grp_properties,
                                 void **prt_properties, float R) const override
    {
        Y_Delta::grp_Y_t m = *(IllustrisFields::Masses::value_type *)
                                (prt_properties[ Y_Delta::PrtF::idx<IllustrisFields::Masses> ]);
        Y_Delta::grp_Y_t e = *(IllustrisFields::InternalEnergy::value_type *)
                                (prt_properties[ Y_Delta::PrtF::idx<IllustrisFields::InternalEnergy> ]);
        Y_Delta::grp_Y_t x = *(IllustrisFields::ElectronAbundance::value_type *)
                                (prt_properties[ Y_Delta::PrtF::idx<IllustrisFields::ElectronAbundance> ]);
        
        return 2.0F * (1.0F+XH) / (1.0F+3.0F*XH+4.0F*XH*x)
               * (gamma-1.0F) * m * e;
    }

    // Compton-Y is additive
    void prt_combine (size_t grp_idx, Y_Delta::grp_Y_t &grp_Y_val, Y_Delta::grp_Y_t prt_Y) const override
    {
        grp_Y_val += prt_Y;
    }

    // we want to store the group masses
    Y_Delta::grp_M_t grp_reduce (void **grp_properties) const override
    {
        return *(IllustrisFields::GroupMass::value_type *)
                    (grp_properties[ Y_Delta::GrpF::idx<IllustrisFields::GroupMass> ]);
    }
};// }}}

template<typename T>
void vec_to_f (const std::vector<T> &v, const std::string &s)
{
    std::FILE *f = std::fopen(s, "wb");
    std::fwrite(v.data(), sizeof(T), v.size(), f);
    std::fclose(f);
}

int main ()
{
    Y_Delta_Callback y;
    
    halo_particles<Y_Delta::GrpF, Y_Delta::PrtF> ( y );

    // save data to files
    vec_to_f<>(y.grp_M, "grp_M.bin");
    vec_to_f<>(y.grp_Y, "grp_Y.bin");

    return 0;
};
