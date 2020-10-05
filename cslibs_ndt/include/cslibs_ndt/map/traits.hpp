#ifndef CSLIBS_NDT_MAP_TRAITS_HPP
#define CSLIBS_NDT_MAP_TRAITS_HPP

#include <cslibs_math_2d/linear/pose.hpp>
#include <cslibs_math_2d/linear/pointcloud.hpp>
#include <cslibs_math_2d/algorithms/ndt_iterator.hpp>
#include <cslibs_math_2d/algorithms/simple_iterator.hpp>
#include <cslibs_math_2d/algorithms/bresenham.hpp>

#include <cslibs_math_3d/linear/pose.hpp>
#include <cslibs_math_3d/linear/pointcloud.hpp>
#include <cslibs_math_3d/algorithms/ndt_iterator.hpp>
#include <cslibs_math_3d/algorithms/simple_iterator.hpp>
#include <cslibs_math_3d/algorithms/bresenham.hpp>

#include <cslibs_indexed_storage/backends.hpp>
#include <cslibs_ndt/backend/octree.hpp>
namespace cis = cslibs_indexed_storage;

namespace cslibs_ndt {
namespace map {

namespace tags {
enum option { static_map, dynamic_map };

template <option o>
struct default_types;

template <>
struct default_types<option::static_map> {
    template<typename data_interface_t_, typename index_interface_t_, typename... options_ts_>
    using default_backend_t = cis::backend::array::Array<data_interface_t_, index_interface_t_, options_ts_...>;
};

template <>
struct default_types<option::dynamic_map> {
    template<typename data_interface_t_, typename index_interface_t_, typename... options_ts_>
    using default_backend_t = cslibs_ndt::backend::OcTree<data_interface_t_, index_interface_t_, options_ts_...>;
};


template <option o>
struct interface_types;

template <>
struct interface_types<option::static_map> {
    template <typename data_t>
    using data_if = cis::interface::sparse<data_t>;
};

template <>
struct interface_types<option::dynamic_map> {
    template <typename data_t>
    using data_if = cis::interface::dense<data_t>;
};
}

template <std::size_t Dim, typename T>
struct traits {};

template <typename T>
struct traits<2,T>
{
    using pose_2d_t             = cslibs_math_2d::Pose2<T>;
    using pose_t                = cslibs_math_2d::Pose2<T>;
    using transform_t           = cslibs_math_2d::Transform2<T>;
    using point_t               = cslibs_math_2d::Point2<T>;
    using pointcloud_t          = cslibs_math_2d::Pointcloud2<T>;
    using default_iterator_t    = cslibs_math_2d::algorithms::NDTIterator<T>;
};

template <typename T>
struct traits<3,T>
{
    using pose_2d_t             = cslibs_math_2d::Pose2<T>;
    using pose_t                = cslibs_math_3d::Pose3<T>;
    using transform_t           = cslibs_math_3d::Transform3<T>;
    using point_t               = cslibs_math_3d::Point3<T>;
    using pointcloud_t          = cslibs_math_3d::Pointcloud3<T>;
    using default_iterator_t    = cslibs_math_3d::algorithms::NDTIterator<T>;
};

}
}

#endif // CSLIBS_NDT_MAP_TRAITS_HPP
