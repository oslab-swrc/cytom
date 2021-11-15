// SPDX-License-Identifier: MIT

#pragma once

#define ALGORITHM_TEMPLATE_WEIGHTED_CC(classname)  \
template class classname<float, algorithm::PageRank, false>; \
template class classname<PageRankDeltaVertexType_t, algorithm::PageRankDelta, false>; \
template class classname<uint32_t, algorithm::BFS, false>; \
template class classname<uint32_t, algorithm::BFSAsync, false>; \
template class classname<uint32_t, algorithm::CC, false>; \
template class classname<uint32_t, algorithm::CCAsync, false>; \
template class classname<float, algorithm::SSSP, true>; \
template class classname<float, algorithm::SSSPAsync, true>; \
template class classname<float, algorithm::SSWP, true>;

#define ALGORITHM_TEMPLATE_WEIGHTED_H(classname)  \
extern template class classname<float, algorithm::PageRank, false>; \
extern template class classname<PageRankDeltaVertexType_t, algorithm::PageRankDelta, false>; \
extern template class classname<uint32_t, algorithm::BFS, false>; \
extern template class classname<uint32_t, algorithm::BFSAsync, false>; \
extern template class classname<uint32_t, algorithm::CC, false>; \
extern template class classname<uint32_t, algorithm::CCAsync, false>; \
extern template class classname<float, algorithm::SSSP, false>; \
extern template class classname<float, algorithm::SSSPAsync, false>; \
extern template class classname<float, algorithm::SSWP, false>;

#define ALGORITHM_TEMPLATE_SINGLE_DATATYPE_CC(classname)  \
template class classname<float>; \
template class classname<PageRankDeltaVertexType_t>; \
template class classname<uint32_t>;

#define ALGORITHM_TEMPLATE_SINGLE_DATATYPE_H(classname)  \
extern template class classname<float>; \
extern template class classname<PageRankDeltaVertexType_t>; \
extern template class classname<uint32_t>;
