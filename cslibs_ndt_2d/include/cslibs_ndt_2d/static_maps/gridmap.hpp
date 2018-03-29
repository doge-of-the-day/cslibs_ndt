#ifndef CSLIBS_NDT_2D_STATIC_MAPS_GRIDMAP_HPP
#define CSLIBS_NDT_2D_STATIC_MAPS_GRIDMAP_HPP

#include <array>
#include <vector>
#include <cmath>
#include <memory>

#include <cslibs_math_2d/linear/pose.hpp>
#include <cslibs_math_2d/linear/point.hpp>

#include <cslibs_ndt/common/distribution.hpp>
#include <cslibs_ndt/common/bundle.hpp>

#include <cslibs_math/linear/pointcloud.hpp>
#include <cslibs_math/common/array.hpp>
#include <cslibs_math/common/div.hpp>
#include <cslibs_math/common/mod.hpp>

#include <cslibs_indexed_storage/storage.hpp>
#include <cslibs_indexed_storage/backend/array/array.hpp>

#include <cslibs_utility/synchronized/wrap_around.hpp>

namespace cis = cslibs_indexed_storage;

namespace cslibs_ndt_2d {
namespace static_maps {
class Gridmap
{
public:
    using Ptr                               = std::shared_ptr<Gridmap>;
    using pose_t                            = cslibs_math_2d::Pose2d;
    using transform_t                       = cslibs_math_2d::Transform2d;
    using point_t                           = cslibs_math_2d::Point2d;
    using index_t                           = std::array<int, 2>;
    using size_t                            = std::array<std::size_t, 2>;
    using mutex_t                           = std::mutex;
    using lock_t                            = std::unique_lock<mutex_t>;
    using distribution_t                    = cslibs_ndt::Distribution<2>;
    using distribution_storage_t            = cis::Storage<distribution_t, index_t, cis::backend::array::Array>;
    using distribution_storage_ptr_t        = std::shared_ptr<distribution_storage_t>;
    using distribution_storage_array_t      = std::array<distribution_storage_ptr_t, 4>;
    using distribution_bundle_t             = cslibs_ndt::Bundle<distribution_t*, 4>;
    using distribution_const_bundle_t       = cslibs_ndt::Bundle<const distribution_t*, 4>;
    using distribution_bundle_storage_t     = cis::Storage<distribution_bundle_t, index_t, cis::backend::array::Array>;
    using distribution_bundle_storage_ptr_t = std::shared_ptr<distribution_bundle_storage_t>;

    Gridmap(const pose_t        &origin,
            const double         resolution,
            const size_t        &size) :
        resolution_(resolution),
        resolution_inv_(1.0 / resolution_),
        bundle_resolution_(0.5 * resolution_),
        bundle_resolution_inv_(1.0 / bundle_resolution_),
        w_T_m_(origin),
        m_T_w_(w_T_m_.inverse()),
        size_(size),
        storage_{{distribution_storage_ptr_t(new distribution_storage_t),
                  distribution_storage_ptr_t(new distribution_storage_t),
                  distribution_storage_ptr_t(new distribution_storage_t),
                  distribution_storage_ptr_t(new distribution_storage_t)}},
        bundle_storage_(new distribution_bundle_storage_t)
    {
        storage_[0]->template set<cis::option::tags::array_size>(size[0],
                                                                 size[1]);
        for(std::size_t i = 1 ; i < 4 ; ++i) {
            storage_[i]->template set<cis::option::tags::array_size>(size[0] + 1,
                                                                     size[1] + 1);
        }
        bundle_storage_->template set<cis::option::tags::array_size>(size[0] * 2,
                                                                     size[1] * 2);
    }

    Gridmap(const double  origin_x,
            const double  origin_y,
            const double  origin_phi,
            const double  resolution,
            const size_t &size) :
        resolution_(resolution),
        resolution_inv_(1.0 / resolution_),
        bundle_resolution_(0.5 * resolution_),
        bundle_resolution_inv_(1.0 / bundle_resolution_),
        w_T_m_(origin_x, origin_y, origin_phi),
        m_T_w_(w_T_m_.inverse()),
        size_(size),
        storage_{{distribution_storage_ptr_t(new distribution_storage_t),
                  distribution_storage_ptr_t(new distribution_storage_t),
                  distribution_storage_ptr_t(new distribution_storage_t),
                  distribution_storage_ptr_t(new distribution_storage_t)}},
        bundle_storage_(new distribution_bundle_storage_t)
    {
        storage_[0]->template set<cis::option::tags::array_size>(size[0],
                                                                 size[1]);
        for(std::size_t i = 1 ; i < 4 ; ++i) {
            storage_[i]->template set<cis::option::tags::array_size>(size[0] + 1,
                                                                     size[1] + 1);
        }
        bundle_storage_->template set<cis::option::tags::array_size>(size[0] * 2,
                                                                     size[1] * 2);
    }

    inline pose_t getOrigin() const
    {
        return w_T_m_;
    }

    inline void add(const point_t &p)
    {
        distribution_bundle_t *bundle;
        {
            lock_t(bundle_storage_mutex_);
            const index_t bi = toBundleIndex(p);
            bundle = getAllocate(bi);
        }
        bundle->at(0)->getHandle()->data().add(p);
        bundle->at(1)->getHandle()->data().add(p);
        bundle->at(2)->getHandle()->data().add(p);
        bundle->at(3)->getHandle()->data().add(p);
    }

    inline void insert(const pose_t &origin,
                       const typename cslibs_math::linear::Pointcloud<point_t>::Ptr &points)
    {
        distribution_storage_t storage;
        for (const auto &p : *points) {
            const point_t pm = origin * p;
            if (pm.isNormal()) {
                const index_t &bi = toBundleIndex(pm);
                distribution_t *d = storage.get(bi);
                (d ? d : &storage.insert(bi, distribution_t()))->data().add(pm);
            }
        }

        storage.traverse([this](const index_t& bi, const distribution_t &d) {
            distribution_bundle_t *bundle;
            {
                lock_t(bundle_storage_mutex_);
                bundle = getAllocate(bi);
            }
            bundle->at(0)->getHandle()->data() += d.data();
            bundle->at(1)->getHandle()->data() += d.data();
            bundle->at(2)->getHandle()->data() += d.data();
            bundle->at(3)->getHandle()->data() += d.data();
        });
    }

    inline double sample(const point_t &p) const
    {
        const index_t bi = toBundleIndex(p);
        return sample(p, bi);
    }

    inline double sample(const point_t &p,
                         const index_t &bi) const
    {
        distribution_bundle_t *bundle;
        {
            lock_t(bundle_storage_mutex_);
            bundle = getAllocate(bi);
        }
        auto evaluate = [&p, &bundle]() {
            return 0.25 * (bundle->at(0)->getHandle()->data().sample(p) +
                           bundle->at(1)->getHandle()->data().sample(p) +
                           bundle->at(2)->getHandle()->data().sample(p) +
                           bundle->at(3)->getHandle()->data().sample(p));
        };
        return evaluate();
    }

    inline double sampleNonNormalized(const point_t &p) const
    {
        const index_t bi = toBundleIndex(p);
        return sampleNonNormalized(p, bi);
    }

    inline double sampleNonNormalized(const point_t &p,
                                      const index_t &bi) const
    {
        distribution_bundle_t *bundle;
        {
            lock_t(bundle_storage_mutex_);
            bundle = getAllocate(bi);
        }
        auto evaluate = [&p, &bundle]() {
            return 0.25 * (bundle->at(0)->getHandle()->data().sampleNonNormalized(p) +
                           bundle->at(1)->getHandle()->data().sampleNonNormalized(p) +
                           bundle->at(2)->getHandle()->data().sampleNonNormalized(p) +
                           bundle->at(3)->getHandle()->data().sampleNonNormalized(p));
        };
        return evaluate();
    }

    inline const distribution_bundle_t* getDistributionBundle(const index_t &bi) const
    {
        return getAllocate(bi);
    }

    inline distribution_bundle_t* getDistributionBundle(const index_t &bi)
    {
        return getAllocate(bi);
    }

    inline double getBundleResolution() const
    {
        return bundle_resolution_;
    }

    inline double getResolution() const
    {
        return resolution_;
    }

    inline double getHeight() const
    {
        return size_[1] * resolution_;
    }

    inline double getWidth() const
    {
        return size_[0] * resolution_;
    }

    inline size_t getSize() const
    {
        return size_;
    }

    inline size_t getBundleSize() const
    {
        return {{size_[0] * 2, size_[1] * 2}};
    }

    inline distribution_storage_array_t const & getStorages() const
    {
        return storage_;
    }

    inline void getBundleIndices(std::vector<index_t> &indices) const
    {
        lock_t(bundle_storage_mutex_);
        auto add_index = [&indices](const index_t &i, const distribution_bundle_t &d) {
            indices.emplace_back(i);
        };
        bundle_storage_->traverse(add_index);
    }

    inline std::size_t getByteSize() const
    {
        lock_t(bundle_storage_mutex_);
        return sizeof(*this) +
                bundle_storage_->byte_size() +
                storage_[0]->byte_size() +
                storage_[1]->byte_size() +
                storage_[2]->byte_size() +
                storage_[3]->byte_size();
    }

protected:
    const double                                    resolution_;
    const double                                    resolution_inv_;
    const double                                    bundle_resolution_;
    const double                                    bundle_resolution_inv_;
    const transform_t                               w_T_m_;
    const transform_t                               m_T_w_;
    const size_t                                    size_;

    mutable mutex_t                                 storage_mutex_;
    mutable distribution_storage_array_t            storage_;
    mutable mutex_t                                 bundle_storage_mutex_;
    mutable distribution_bundle_storage_ptr_t       bundle_storage_;

    inline distribution_t* getAllocate(const distribution_storage_ptr_t &s,
                                       const index_t &i) const
    {
        lock_t(storage_mutex_);
        distribution_t *d = s->get(i);
        return d ? d : &(s->insert(i, distribution_t()));
    }

    inline distribution_bundle_t *getAllocate(const index_t &bi) const
    {
        distribution_bundle_t *bundle;
        {
            lock_t(bundle_storage_mutex_);
            bundle = bundle_storage_->get(bi);
        }

        auto allocate_bundle = [this, &bi]() {
            distribution_bundle_t b;
            const int divx = cslibs_math::common::div(bi[0], 2);
            const int divy = cslibs_math::common::div(bi[1], 2);
            const int modx = cslibs_math::common::mod(bi[0], 2);
            const int mody = cslibs_math::common::mod(bi[1], 2);

            const index_t storage_0_index = {{divx,        divy}};
            const index_t storage_1_index = {{divx + modx, divy}};        /// shifted to the left
            const index_t storage_2_index = {{divx,        divy + mody}}; /// shifted to the bottom
            const index_t storage_3_index = {{divx + modx, divy + mody}}; /// shifted diagonally

            b[0] = getAllocate(storage_[0], storage_0_index);
            b[1] = getAllocate(storage_[1], storage_1_index);
            b[2] = getAllocate(storage_[2], storage_2_index);
            b[3] = getAllocate(storage_[3], storage_3_index);

            lock_t(bundle_storage_mutex_);
            return &(bundle_storage_->insert(bi, b));
        };

        return bundle == nullptr ? allocate_bundle() : bundle;
    }


    inline index_t toBundleIndex(const point_t &p_w) const
    {
        const point_t p_m = m_T_w_ * p_w;
        return {{static_cast<int>(std::floor(p_m(0) * bundle_resolution_inv_)),
                 static_cast<int>(std::floor(p_m(1) * bundle_resolution_inv_))}};
    }
};
}
}
#endif // CSLIBS_NDT_2D_STATIC_MAPS_GRIDMAP_HPP
