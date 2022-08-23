// Copyright 2013-2020 Lawrence Livermore National Security, LLC and other
// HavoqGT Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <chrono>
#include <deque>
#include <limits>
#include <unordered_set>

#include <havoqgt/visitor_queue.hpp>
#include <havoqgt/detail/visitor_priority_queue.hpp>

// timer
#include <signal.h>    /* union sigval / struct sigevent */
#include <stdio.h>    /* printf */
#include <string.h>    /* memset */
#include <unistd.h> /* sleep */
#include <time.h>
#include "config.hpp"

// #define TDS_TIME_LIMIT 300
timer_t id_nlcc;
void set_stop_flag_nlcc(union sigval val)
{
    // printf("%d\n",val.sival_int);
    // printf("%p\n",val.sival_ptr);
    *(bool*)val.sival_ptr = true;
    timer_delete(id_nlcc);
}

void register_timer_nlcc(bool& stop_flag, int time_limit){
    struct timespec spec;
    struct sigevent ent;
    struct itimerspec value;
    struct itimerspec get_val;

    /* Init */
    memset(&ent, 0x00, sizeof(struct sigevent));
    memset(&get_val, 0x00, sizeof(struct itimerspec));
    
    int test_val = 0;
    /* create a timer */
    ent.sigev_notify = SIGEV_THREAD;
    ent.sigev_notify_function = set_stop_flag_nlcc;
    ent.sigev_value.sival_ptr = &stop_flag;
    // printf("create timer\n");
    timer_create(CLOCK_MONOTONIC, &ent, &id_nlcc);

    /* start a timer */
    value.it_value.tv_sec = time_limit;
    value.it_value.tv_nsec = 0;
    value.it_interval.tv_sec = 0;
    value.it_interval.tv_nsec = 0;
    // printf("start timer\n");
    timer_settime(id_nlcc, 0, &value, NULL);
}



///namespace havoqgt { ///namespace mpi {

namespace prunejuice
{

    // TODO: improve
    static bool  enable_token_aggregation_nlcc  = true;
    static bool  enable_vertex_token_source_cache_nlcc = false; // TODO: delete
    static bool path_checking_filter_nlcc = false;             //true;

    template <typename Visitor>
    class tppm_queue_nlcc
    {

    public:
        tppm_queue_nlcc() {}

        bool push(Visitor const &element)
        {
            data.push_back(element);
            return true;
        }

        void pop()
        {
            //data.pop_back();
            data.pop_front();
        }

        Visitor const &top()
        {
            //return data.back();
            return data.front();
        }

        size_t size() const
        {
            return data.size();
            ;
        }

        bool empty() const
        {
            return data.empty();
        }

        void clear()
        {
            data.clear();
        }

    protected:
        //std::vector<Visitor> data;
        std::deque<Visitor> data;
    };

    // token passing pattern matching visitor class
    template <typename Graph, typename Vertex, typename BitSet>
    class tppm_visitor_nlcc
    {

    public:
        typedef typename Graph::vertex_locator vertex_locator;
        typedef typename Graph::edge_iterator eitr_type;

        tppm_visitor_nlcc() : itr_count(0),
                             do_pass_token(false),
                             is_init_step(true),
                             ack_success(false),
                             //msg_type(0),
                             source_index_pattern_indices(0),
                             parent_pattern_index(0)
        {
            visited_vertices[itr_count] = vertex;
        }

        tppm_visitor_nlcc(vertex_locator _vertex) : vertex(_vertex),
                                                   itr_count(0),
                                                   do_pass_token(false),
                                                   is_init_step(true),
                                                   ack_success(false),
                                                   //msg_type(0),
                                                   source_index_pattern_indices(0),
                                                   parent_pattern_index(0)
        {
            visited_vertices[itr_count] = vertex;
        }

        template <typename VertexLocatorArrayStatic>
        tppm_visitor_nlcc(vertex_locator _vertex,
                         vertex_locator _parent,
                         vertex_locator _target_vertex,
                         Vertex _vertex_label,
                         Vertex _target_vertex_label,
                         uint64_t _sequence_number,
                         VertexLocatorArrayStatic &_visited_vertices,
                         size_t _itr_count,
                         size_t _max_itr_count,
                         size_t _source_index_pattern_indices,
                         size_t _parent_pattern_index,
                         bool _expect_target_vertex = true,
                         bool _do_pass_token = true,
                         bool _is_init_step = false,
                         bool _ack_success = false //,
                         //uint8_t _msg_type = 1
                         ) :

                             vertex(_vertex),
                             parent(_parent),
                             target_vertex(_target_vertex),
                             vertex_label(_vertex_label),
                             target_vertex_label(_target_vertex_label),
                             sequence_number(_sequence_number),
                             itr_count(_itr_count),
                             max_itr_count(_max_itr_count),
                             expect_target_vertex(_expect_target_vertex),
                             do_pass_token(_do_pass_token),
                             is_init_step(_is_init_step),
                             ack_success(_ack_success),
                             // msg_type(_msg_type),
                             source_index_pattern_indices(_source_index_pattern_indices),
                             parent_pattern_index(_parent_pattern_index)
        {
            //if (itr_count == 0 && !_ack_success) { // probably never gets here
            //  visited_vertices[itr_count] = vertex;
            //} else if (itr_count > 0 && itr_count <= max_itr_count && !_ack_success) {
            if (!_ack_success)
            {
                // copy to visited_vertices from the one received from the parent
                std::copy(std::begin(_visited_vertices),
                          std::end(_visited_vertices), // TODO: std::begin(_visited_vertices) + itr_count ?
                          std::begin(visited_vertices));
                visited_vertices[itr_count + 1] = vertex; // Important : itr_count for the next hop
            }
        }

        template <typename AlgData>
        bool pre_visit(AlgData &alg_data) const
        {
            if(std::get<23>(alg_data)){ //overtime
                return true;
            }

            if (!std::get<13>(alg_data)[vertex])
            {
                return false;
            }

            ///int mpi_rank = havoqgt_env()->world_comm().rank();
            int mpi_rank = havoqgt::comm_world().rank();

            auto vertex_data = std::get<0>(alg_data)[vertex];
            auto &pattern = std::get<1>(alg_data);
            auto &pattern_indices = std::get<2>(alg_data);
            //auto& pattern_graph = std::get<4>(alg_data);
            auto g = std::get<11>(alg_data); // graph

            if (ack_success)
            {
                return true; // Important : must return true to handle delegates
            }

            // TODO: remove ?
            if ( enable_vertex_token_source_cache_nlcc)
            {
                // verify if this vertex have already forwarded a token from the originating vertex
                if (!is_init_step && max_itr_count > itr_count)
                {
                    auto find_token_source_forwarded = std::get<12>(alg_data)[vertex].find(g->locator_to_label(target_vertex));
                    if (find_token_source_forwarded != std::get<12>(alg_data)[vertex].end())
                    {
                        return false;
                    }
                }
            } // enable_vertex_token_source_cache_nlcc

            if (!do_pass_token && is_init_step && itr_count == 0)
            {
                // probably it never gets here
                if (vertex_data == pattern[0])
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
            else if (!is_init_step)
            {
                // forward tokens

                // target_vertex cannot relay the token
                //if (do_pass_token && (max_itr_count > itr_count) &&
                //  (g->locator_to_label(vertex) == g->locator_to_label(target_vertex))) {
                //  return false;
                //}

                auto new_itr_count = itr_count + 1;
                auto next_pattern_index = source_index_pattern_indices + new_itr_count; // expected next pattern_index
                // TODO: notice next_pattern_index is redundent now, same as new_itr_count; just use new_itr_count
                auto vertex_pattern_index = 0; //find_vertex->second.vertex_pattern_index;
                vertex_pattern_index = pattern_indices[next_pattern_index];

                // TODO: read vertex_pattern_index from template_vertices
                // vertex_data == pattern[next_pattern_index] and vertex_pattern_index == pattern_indices[next_pattern_index] must hold
                // otherwise return false

                // verify vertex data
                // verify if received from a valid parent
                if (vertex_data != pattern[next_pattern_index] &&
                    parent_pattern_index != pattern_indices[next_pattern_index - 1])
                {
                    return false;
                }

                bool match_found = false;

                BitSet vertex_template_vertices(std::get<14>(alg_data)[vertex]); // template_vertices

                if (vertex_template_vertices.none() ||
                    !vertex_template_vertices.test(pattern_indices[next_pattern_index]))
                {
                    return false;
                }
                else
                {
                    match_found = true;
                }

                if (!match_found)
                {
                    return false;
                }

                // verify if received from a valid parent // TODO: remove redundent checks
                if (vertex_data == pattern[next_pattern_index] &&
                    vertex_pattern_index == pattern_indices[next_pattern_index])
                {
                    if (vertex_data == pattern[next_pattern_index] &&
                        parent_pattern_index == pattern_indices[next_pattern_index - 1])
                    {
                        if (do_pass_token && (max_itr_count > itr_count))
                        {

                            if ( enable_token_aggregation_nlcc )
                            {
                                //auto find_token = std::get<19>(alg_data)->find(*this);
                                //if (find_token == std::get<19>(alg_data)->end()) {
                                auto find_token = std::get<19>(alg_data)[vertex].find(*this);
                                if (find_token == std::get<19>(alg_data)[vertex].end())
                                {

                                    // not found
                                }
                                else
                                {
                                    return false;
                                }
                            } //  enable_token_aggregation_nlcc 

                            // TODO: remove
                            if ( enable_vertex_token_source_cache_nlcc)
                            {
                                auto find_token_source_forwarded = std::get<12>(alg_data)[vertex].find(g->locator_to_label(target_vertex));
                                if (find_token_source_forwarded == std::get<12>(alg_data)[vertex].end())
                                {
                                    auto insert_status = std::get<12>(alg_data)[vertex].insert(g->locator_to_label(target_vertex));
                                    if (!insert_status.second)
                                    {
                                        std::cerr << "Error: failed to add an element to the set." << std::endl;
                                        return false;
                                    }
                                    // std::cout << g.locator_to_label(vertex) << " adding " << g.locator_to_label(target_vertex)
                                    //  << " to the vertex set" << std::endl; // Test
                                }
                                else
                                {
                                    std::cerr << "Error: unexpected item in the set." << std::endl;
                                    return false;
                                }
                            } //  enable_vertex_token_source_cache_nlcc


                            if (std::get<18>(alg_data)[new_itr_count] == new_itr_count)
                            {
                                // duplicate is not expected in visited_vertices
                                for (size_t i = 0; i < new_itr_count; i++)
                                { // Important : must be < not <=
                                    if (g->locator_to_label(visited_vertices[i]) == g->locator_to_label(vertex))
                                    {
                                        //break;
                                        //std::cout << g->locator_to_label(vertex) << " returning false from pre_visit ... " << std::endl; // Test
                                        return false;
                                    }
                                }
                            }
                            else if (std::get<18>(alg_data)[new_itr_count] < new_itr_count)
                            {
                                // vertex is expected in visited_vertices
                                if (g->locator_to_label(visited_vertices[std::get<18>(alg_data)[new_itr_count]]) != g->locator_to_label(vertex))
                                {
                                    return false;
                                }
                            }
                            else
                            {
                                std::cerr << "Error: invalid value A." << std::endl;
                                return false;
                            } // TDS

                            if ( enable_token_aggregation_nlcc )
                            {
                                // add to the visitor set
                                //auto find_token = std::get<19>(alg_data)->find(*this); // do not need this here
                                //auto insert_status = std::get<19>(alg_data)->insert(*this);
                                auto insert_status = std::get<19>(alg_data)[vertex].insert(*this);
                                if (!insert_status.second)
                                {
                                    //std::cerr << "MPI Rank: " << mpi_rank << " "
                                    //  << g->locator_to_label(vertex) << " " << g->locator_to_label(target_vertex)
                                    // << " Error: failed to add an element to the set." << std::endl; // Test
                                    std::cerr << "Error: failed to add an element to the set." << std::endl;
                                    return false;
                                }
                                else
                                {
                                    //std::cout << "MPI Rank: " << mpi_rank << " "
                                    //  << g->locator_to_label(vertex) << " " << g->locator_to_label(target_vertex)
                                    //  << " is delegate slave " << is_vertex_delegate_slave << " "
                                    //  << " Added to the visitor set."
                                    //  << " Pointer address " << std::get<19>(alg_data)
                                    //  << " size " << std::get<19>(alg_data)->size()
                                    //  << std::endl; // Test
                                }
                            } //  enable_token_aggregation_nlcc 
                        }     // max_itr_count > itr_count // TODO: || max_itr_count == itr_count ?

                        return true; // delegate forwarding to the controller, local and controller invoking visit
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
            //++    return true;
            return false; // Test
        }

        template <typename VisitorQueueHandle, typename AlgData>
        bool init_visit(Graph &g, VisitorQueueHandle vis_queue,
                        AlgData &alg_data) const
        {
            if (!std::get<13>(alg_data)[vertex])
            {
                return false;
            }
            else
            {
                return visit(g, vis_queue, alg_data);
            }
        }

        template <typename VisitorQueueHandle, typename AlgData>
        bool visit(Graph &g, VisitorQueueHandle vis_queue, AlgData &alg_data) const
        {
            if(std::get<23>(alg_data)){ //overtime
                if(ack_success){
                    auto find_token_source = std::get<6>(alg_data).find(g.locator_to_label(vertex)); // token_source_map
                    if (find_token_source == std::get<6>(alg_data).end())
                    {
                        std::cerr << "Error: did not find the expected item in the map."
                                  << std::endl;
                        //return true; // false ?
                        return false;
                    }
                    find_token_source->second = 1; //true;
                    std::get<9>(alg_data) = 1;
                    return false;
                }
                tppm_visitor_nlcc new_visitor(target_vertex, vertex, target_vertex,
                                                         g.locator_to_label(target_vertex), // vertex_label
                                                         g.locator_to_label(vertex),        //g.locator_to_label(target_vertex), // token_label
                                                         std::get<21>(alg_data)[vertex],    //sequence_number
                                                         visited_vertices, itr_count, max_itr_count,
                                                         source_index_pattern_indices, 0, expect_target_vertex, false, false, true);
                vis_queue->queue_visitor(new_visitor);
                return false;
            }

            if (!std::get<13>(alg_data)[vertex])
            {
                return false;
            }

            ///int mpi_rank = havoqgt_env()->world_comm().rank();
            int mpi_rank = havoqgt::comm_world().rank();

            if (vertex.is_delegate() && (g.master(vertex) != mpi_rank))
            {
                // Important : it should never get here
                std::cerr << "Error: Controller forwarded visitor to delegates."
                          << std::endl;
                return false;
            }

            if (ack_success)
            {
                // token source received ack_success
                auto find_token_source = std::get<6>(alg_data)
                                             .find(g.locator_to_label(vertex)); // token_source_map
                if (find_token_source == std::get<6>(alg_data).end())
                {
                    std::cerr << "Error: did not find the expected item in the map."
                              << std::endl;
                    //return true; // false ?
                    return false;
                }
                find_token_source->second = 1; //true;
                std::get<9>(alg_data) = 1;     // true; // pattern_found
                                               //++      return true; // Important : must return true to handle delegates
                return false;
            }

            // if vertex is a delegate
            if (!is_init_step && vertex.is_delegate() && (g.master(vertex) != mpi_rank))
            {
                // verify if this vertex has already forwarded a token from the originating vertex

                if ( enable_vertex_token_source_cache_nlcc)
                {
                    if (!is_init_step && max_itr_count > itr_count)
                    {
                        auto find_token_source_forwarded = std::get<12>(alg_data)[vertex].find(g.locator_to_label(target_vertex));
                        if (find_token_source_forwarded != std::get<12>(alg_data)[vertex].end())
                        {
                            return false;
                        }
                    }
                } //  enable_vertex_token_source_cache_nlcc
            }

            auto vertex_data = std::get<0>(alg_data)[vertex];
            auto &pattern = std::get<1>(alg_data);
            auto &pattern_indices = std::get<2>(alg_data);
            //auto& pattern_graph = std::get<4>(alg_data);

            auto pattern_cycle_length = std::get<7>(alg_data);
            auto pattern_valid_cycle = std::get<8>(alg_data);

            if (!do_pass_token && is_init_step && itr_count == 0)
            {
                // init tokens from the source vertex

                // batch token passing
                // if vertex is in the token_source_map, initiate a token from it
                auto find_token_source = std::get<6>(alg_data).find(g.locator_to_label(vertex)); // token_source_map
                if (find_token_source == std::get<6>(alg_data).end())
                {
                    return false;
                }

                if (vertex_data != pattern[0])
                {
                    return false;
                }

                // pattern_selected_vertices and vertex_token_source_set
                if (std::get<16>(alg_data) && std::get<12>(alg_data)[vertex].empty())
                {
                    return false;
                }

                BitSet vertex_template_vertices(std::get<14>(alg_data)[vertex]); // template_vertices

                if (vertex_template_vertices.none() || !vertex_template_vertices.test(pattern_indices[0]))
                {
                    return false;
                }

                for (auto &item : std::get<15>(alg_data)[vertex])
                { // vertex_active_edges_map
                    vertex_locator neighbour = g.label_to_locator(item.first);

                    if (std::get<16>(alg_data))
                    { // pattern_selected_vertices
                        // vertex_token_source_set // the set was populate in the previous iteration
                        // std::cout << "Token source: " << g.locator_to_label(vertex) << " " << std::get<12>(alg_data)[vertex].size() << std::endl; // Test
                        for (auto v : std::get<12>(alg_data)[vertex])
                        {
                            tppm_visitor_nlcc new_visitor(neighbour, vertex, g.label_to_locator(v),
                                                         //g.locator_to_label(neighbour), // vertex_label
                                                         v, // vertex_label
                                                         v, // token_label
                                                         0, // sequence_number
                                                         visited_vertices, 0, pattern_cycle_length, 0, pattern_indices[0], pattern_valid_cycle, true, false);
                            vis_queue->queue_visitor(new_visitor);
                        }
                    }
                    else
                    {
                        tppm_visitor_nlcc new_visitor(neighbour, vertex, vertex,
                                                     //g.locator_to_label(neighbour), // vertex_label
                                                     g.locator_to_label(vertex),     // vertex_label
                                                     g.locator_to_label(vertex),     // token_label
                                                     std::get<21>(alg_data)[vertex], //0, // sequence_number
                                                     visited_vertices, 0, pattern_cycle_length, 0, pattern_indices[0], pattern_valid_cycle, true, false);
                        vis_queue->queue_visitor(new_visitor);
                    }
                }
                //++      return true;
                return false; // controller not forwarding visitor to delegates
            }
            else if (!is_init_step)
            {
                // forward tokens
                bool do_forward_token = false;
                auto new_itr_count = itr_count + 1;                                     // Important : itr_count is the itr_count of the parent
                auto next_pattern_index = source_index_pattern_indices + new_itr_count; // expected next pattern_index
                auto vertex_pattern_index = 0;                                          //find_vertex->second.vertex_pattern_index;
                vertex_pattern_index = pattern_indices[next_pattern_index];
                if (vertex_data != pattern[next_pattern_index])
                {
                    return false;
                }

                bool match_found = false;

                BitSet vertex_template_vertices(std::get<14>(alg_data)[vertex]); // template_vertices

                if (vertex_template_vertices.none() ||
                    !vertex_template_vertices.test(pattern_indices[next_pattern_index]))
                {
                    return false;
                }
                else
                {
                    match_found = true;
                }
                if (!match_found)
                {
                    return false;
                }
                // TODO: verify next_pattern_index < pattern_indices.size() before anythin else

                if (max_itr_count > itr_count)
                {
                    if (vertex_data == pattern[next_pattern_index] &&
                        vertex_pattern_index == pattern_indices[next_pattern_index])
                    {
                        if (parent_pattern_index == pattern_indices[next_pattern_index - 1])
                        {
                            do_forward_token = true;
                        }
                    } // if

                    // TDS
                    //std::get<18>(alg_data); // pattern_enumeration_indices
                    if (std::get<18>(alg_data)[new_itr_count] == new_itr_count)
                    {
                        // duplicate is not expected in visited_vertices
                        for (size_t i = 0; i < new_itr_count; i++)
                        { // Important : must be < not <=
                            if (g.locator_to_label(visited_vertices[i]) == g.locator_to_label(vertex))
                            {
                                //break;
                                return false;
                            }
                        }
                    }
                    else if (std::get<18>(alg_data)[new_itr_count] < new_itr_count)
                    {
                        // vertex is expected in visited_vertices
                        if (g.locator_to_label(visited_vertices[std::get<18>(alg_data)[new_itr_count]]) != g.locator_to_label(vertex))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        std::cerr << "Error: invalid value B." << std::endl;
                        return false;
                    }
                }
                else if (max_itr_count == itr_count)
                {
                    // are vertex_data and vertex_pattern_index valid
                    //bool match_found = false;
                    match_found = false;

                    // verify if received from a valid parent
                    //        if (vertex_data == pattern[pattern_indices[next_pattern_index]] &&
                    // TODO: Important: for token passing only
                    if (vertex_data == pattern[next_pattern_index] &&
                        vertex_pattern_index == pattern_indices[next_pattern_index])
                    {
                        // verify if received from a valid parent
                        //          for (auto e = pattern_graph.vertices[vertex_pattern_index];
                        //            e < pattern_graph.vertices[vertex_pattern_index + 1]; e      return false;) {
                        //            if (pattern_graph.edges[e] == parent_pattern_index) {
                        //              match_found = true;
                        //              break;
                        //            }
                        //          } // for
                        if (parent_pattern_index == pattern_indices[next_pattern_index - 1])
                        {
                            match_found = true;
                        }
                    } // if

                    if (match_found && !expect_target_vertex)
                    {

                        // path checking
                        if (g.locator_to_label(vertex) == g.locator_to_label(target_vertex))
                        {
                            return false; //true;
                        }
                        else
                        {
                            // valid path found
                            // send ack_success to the token source so it could update its state
                            tppm_visitor_nlcc new_visitor(target_vertex, vertex, target_vertex,
                                                         g.locator_to_label(target_vertex), // vertex_label
                                                         g.locator_to_label(vertex),        //g.locator_to_label(target_vertex), // token_label
                                                         std::get<21>(alg_data)[vertex],    //sequence_number
                                                         visited_vertices, itr_count, max_itr_count,
                                                         source_index_pattern_indices, 0, expect_target_vertex, false, false, true);
                            vis_queue->queue_visitor(new_visitor);
#ifdef ENABLE_TDS
                            
#endif
                            

                            return false; //true;
                        }
                    }

                    else if (g.locator_to_label(vertex) == g.locator_to_label(target_vertex) && (g.locator_to_label(vertex) == g.locator_to_label(visited_vertices[0])) && match_found && expect_target_vertex)
                    {
                        // cycle checking

                        auto find_token_source = std::get<6>(alg_data).find(g.locator_to_label(vertex)); // token_source_map
                        if (find_token_source == std::get<6>(alg_data).end())
                        {
                            std::cerr << "Error: did not find the expected item in the map." << std::endl;
                            //return true; // false ?
                            return false;
                        }

                        find_token_source->second = 1; //true;

                        std::get<9>(alg_data) = 1; // true; // pattern_found

                        // write to the paths_result_file
#ifdef ENABLE_TDS

#endif

                        // Test

                        //++          return true; // Important : must return true to handle delegates
                        return false;
                    }
                    else
                    {
                        std::cerr << "Error: wrong code branch." << std::endl;
                        return false; //true;//false;
                    }
                }
                else
                {
                    std::cerr << "Error: wrong code branch." << std::endl;
                    return false;
                }

                if (!do_forward_token)
                {
                    return false;
                }

                for (auto item : std::get<15>(alg_data)[vertex])
                { // vertex_active_edges_map
                    vertex_locator neighbour = g.label_to_locator(item.first);
                    // penultimate hop, only forward to the target_vertex
                    if (max_itr_count == new_itr_count)
                    {
                        if (expect_target_vertex && (g.locator_to_label(target_vertex) != g.locator_to_label(neighbour)))
                        {
                            // cycle checking
                            continue;
                        }
                        else if (!expect_target_vertex && (g.locator_to_label(target_vertex) == g.locator_to_label(neighbour)))
                        {
                            // path checking
                            continue;
                        }
                        else if (!expect_target_vertex && (g.locator_to_label(target_vertex) != g.locator_to_label(neighbour)))
                        {
                            // path checking
                            match_found = false;

                            //std::get<18>(alg_data); // pattern_enumeration_indices
                            if (std::get<18>(alg_data)[new_itr_count + 1] == new_itr_count + 1)
                            {
                                // duplicate is not expected in visited_vertices
                                for (size_t i = 0; i <= new_itr_count; i++)
                                {
                                    if (g.locator_to_label(visited_vertices[i]) == g.locator_to_label(neighbour))
                                    {
                                        match_found = true;
                                        break;
                                    }
                                }
                            }
                            else if (std::get<18>(alg_data)[new_itr_count + 1] < new_itr_count + 1)
                            {
                                // neighbour is expected in visited_vertices
                                if (g.locator_to_label(visited_vertices[std::get<18>(alg_data)[new_itr_count + 1]]) != g.locator_to_label(neighbour))
                                {
                                    match_found = true;
                                }
                            }
                            else
                            {
                                std::cerr << "Error: invalid value C." << std::endl;
                                //return false;
                                match_found = true;
                            }

                            if (match_found)
                            {
                                continue;
                            }
                        }
                    }
                    else if (max_itr_count > itr_count)
                    {
                        match_found = false;
                        //std::get<18>(alg_data); // pattern_enumeration_indices
                        if (std::get<18>(alg_data)[new_itr_count + 1] == new_itr_count + 1)
                        {
                            // duplicate is not expected in visited_vertices
                            for (size_t i = 0; i <= new_itr_count; i++)
                            {
                                if (g.locator_to_label(visited_vertices[i]) == g.locator_to_label(neighbour))
                                {
                                    match_found = true;
                                    break;
                                }
                            }
                        }
                        else if (std::get<18>(alg_data)[new_itr_count + 1] < new_itr_count + 1)
                        {
                            // neighbour is expected in visited_vertices
                            if (g.locator_to_label(visited_vertices[std::get<18>(alg_data)[new_itr_count + 1]]) != g.locator_to_label(neighbour))
                            {
                                match_found = true;
                            }
                        }
                        else
                        {
                            std::cerr << "Error: invalid value D." << std::endl;
                            //return false;
                            match_found = true;
                        }

                        if (match_found)
                        {
                            continue;
                        }
                    }
                    else
                    {
                        std::cerr << "Error: wrong code branch." << std::endl;
                        return false;
                    }

                    auto new_token_label = target_vertex_label;
                    auto new_sequence_number = sequence_number;
                    //auto temp_enumeration = false; // Test
                    auto enable_new_token_label = std::get<22>(alg_data)[next_pattern_index];

                    auto has_unique_template_vertex =
                        (vertex_template_vertices.count() == 1 ? true : false);

                    if (enable_new_token_label)
                    {
                        new_token_label = g.locator_to_label(vertex),
                        new_sequence_number = std::get<21>(alg_data)[vertex]++;
                    }

                    tppm_visitor_nlcc new_visitor(neighbour, vertex, target_vertex,
                                                 //g.locator_to_label(neighbour), // vertex_label
                                                 g.locator_to_label(target_vertex), // vertex_label
                                                 new_token_label,                   // token_label
                                                 new_sequence_number,               //sequence_number
                                                 visited_vertices,
                                                 new_itr_count, max_itr_count, source_index_pattern_indices, vertex_pattern_index,
                                                 expect_target_vertex);
                    // vertex_pattern_index = parent_pattern_index for the neighbours
                    vis_queue->queue_visitor(new_visitor);
                }

                return false; // controller not forwarding visitor to delegates

                // else if
            }
            else
            {
                return false;
            }
            return false;
        }

        friend inline bool operator>(const tppm_visitor_nlcc &v1, const tppm_visitor_nlcc &v2)
        {
            //return false;
            if (v1.itr_count > v2.itr_count)
            {
                return false;
            }
            else if (v1.itr_count < v2.itr_count)
            {
                return true;
            }
            if (v1.vertex == v2.vertex)
            {
                return true;
            }
            return (v1.vertex < v2.vertex);

            /*if (v1.itr_count <= v2.itr_count) {
      return true;
    } else {
      return false;
    }*/
        }

        //friend inline bool operator<(const tppm_visitor& v1, const tppm_visitor& v2) {
        //return false;
        //if (v1.itr_count < v2.itr_count) {
        //  return true;
        //} else if (v1.itr_count > v2.itr_count) {
        //  return false;
        //}
        //if (v1.vertex == v2.vertex) {
        //  return false;
        //}
        //return !(v1.vertex < v2.vertex);
        //}

        vertex_locator vertex;
        vertex_locator parent;
        vertex_locator target_vertex; // for a cycle, this is also the originating vertex // TODO: remove

        //size_t itr_count; // TODO: change type // Important : itr_count of the parent
        //size_t max_itr_count; // equal to diameter - 1 of the pattern as itr_count is initialized to 0 // TODO: change type
        //bool expect_target_vertex;
        //bool do_pass_token;
        //bool is_init_step;
        //bool ack_success;

        size_t source_index_pattern_indices;             // index of the token source in the pattern_indices container
        size_t parent_pattern_index;                     // TODO: change to the same type as in the pattern_graph
        std::array<vertex_locator, 16> visited_vertices; // I think we will have to replace vertex_locator with Vertex

        Vertex vertex_label;
        Vertex target_vertex_label; // TODO: this label will be updated at the points where no caching happens // token_label?
        uint64_t sequence_number;

        size_t itr_count;     // TODO: change type // Important : itr_count of the parent // TODO: remove
        size_t max_itr_count; // equal to diameter - 1 of the pattern as itr_count is initialized to 0 // TODO: change type

        bool expect_target_vertex; // TODO. remove
        bool do_pass_token;        // TODO: remove
        bool is_init_step;
        bool ack_success;

        // TODO: replace all of the above by msg_type
        uint8_t msg_type; // 0 - init, 1 - foward, 2 - ack-success, 3 - ack-failure
    };

    template <typename TGraph, typename Vertex, typename Edge, typename VertexData,
              typename EdgeData, typename VertexMetadata, typename EdgeMetadata,
              typename VertexActive, typename VertexUint8MapCollection,
              typename TemplateVertex, typename VertexStateMap, typename PatternGraph,
              typename PatternUtilities, typename VertexUint8Map,
              typename VertexSetCollection,
              template <typename> class DelegateGraphVertexDataSTDAllocator,
              typename Boolean, typename BitSet>

    void token_passing_pattern_filtering(TGraph *g, VertexMetadata &vertex_metadata,
                                        VertexActive &vertex_active,
                                        VertexUint8MapCollection &vertex_active_edges_map,
                                        TemplateVertex &template_vertices, VertexStateMap &vertex_state_map,
                                        PatternGraph &pattern_graph, PatternUtilities &pattern_utilities, size_t pl,
                                        VertexUint8Map &token_source_map,
                                        VertexSetCollection &vertex_token_source_set,
                                        std::vector<uint8_t>::reference pattern_found,
                                        std::uint64_t batch_size,
                                        std::ofstream &paths_result_file, uint64_t &message_count)
    {

        ///int mpi_rank = havoqgt_env()->world_comm().rank();
        ///int mpi_size = havoqgt_env()->world_comm().size();
        int mpi_rank = havoqgt::comm_world().rank();
        int mpi_size = havoqgt::comm_world().size();
        //size_t superstep_var = 0;
        //size_t& superstep_ref = superstep_var;

        if (mpi_rank == 0)
        {
            std::cout << "Token Passing tds [" << pl << "] | Template Driven Search ... " << std::endl;
        }

        // TODO: temporary patch
        auto pattern = std::get<0>(pattern_utilities.input_patterns[pl]);
        auto pattern_indices = std::get<1>(pattern_utilities.input_patterns[pl]);
        auto pattern_cycle_length = std::get<2>(pattern_utilities.input_patterns[pl]); // uint
        auto pattern_valid_cycle = std::get<3>(pattern_utilities.input_patterns[pl]);  // boolean
        //auto pattern_interleave_label_propagation = std::get<4>(pattern_utilities.input_patterns[pl]); // boolean
        //--  auto pattern_seleted_edges_tp = std::get<5>(pattern_utilities.input_patterns[pl]); // boolean
        //auto pattern_selected_vertices = std::get<5>(pattern_utilities.input_patterns[pl]); // boolean

        auto pattern_selected_vertices = 0; // TODO: remove

        auto pattern_is_tds = std::get<4>(pattern_utilities.input_patterns[pl]);                       // boolean
        auto pattern_interleave_label_propagation = std::get<5>(pattern_utilities.input_patterns[pl]); // boolean

        auto pattern_enumeration_indices = pattern_utilities.enumeration_patterns[pl];
        auto pattern_aggregation_steps = pattern_utilities.aggregation_steps[pl];
        // TODO: temporary patch

        // TODO: delete
        uint8_t vertex_rank = 0;   // dummy
        uint8_t edge_metadata = 0; // dummy
        uint8_t superstep_var = 0; // dummy
        // TODO: delete

        uint64_t max_itr_count = std::get<2>(pattern_utilities.input_patterns[pl]);
        //uint64_t itr_count = 0; // superstep_var
        bool expect_target_vertex = std::get<3>(pattern_utilities.input_patterns[pl]);
        //Vertex parent_pattern_vertex = ; // a.k.a parent_pattern_index;
        auto source_index_pattern_indices = 0;

        bool enable_filtered_token_relay = false;

        typedef typename TGraph::vertex_iterator vertex_iterator;
        typedef typename TGraph::vertex_locator vertex_locator;

        typedef tppm_visitor_nlcc<TGraph, Vertex, BitSet> visitor_type;

        //typedef std::vector<visitor_type> VectorVisitor;
        //typedef DelegateGraphVertexDataSTDAllocator<VectorVisitor> VertexVisitorCollection;
        //VertexVisitorCollection vertex_visitors(*g);

        typedef DelegateGraphVertexDataSTDAllocator<uint64_t> VertexUint64Collection;
        VertexUint64Collection vertex_sequence_number(*g);

        struct VisitorCompare_2
        {
        public:
            // WDC_C_4#8
            Boolean aggregation_steps[8][8] = {
                {0, 0, 0, 0, 0, 0, 0, 0},
                {1, 0, 0, 0, 0, 0, 0, 0},
                {1, 1, 0, 0, 0, 0, 0, 0},
                {1, 1, 1, 0, 0, 0, 0, 0},
                {1, 1, 0, 1, 0, 0, 0, 0},
                {1, 1, 0, 1, 0, 0, 0, 0},
                {1, 1, 0, 0, 0, 0, 0, 0},
                {0, 0, 0, 0, 0, 0, 0, 0}};

            bool operator()(const visitor_type &v1, const visitor_type &v2) const
            {
                /*        if (v1.itr_count != v2.itr_count) {
          return false;
        }

        if (v1.visited_vertices[0] != v2.visited_vertices[0]) {
          return false;     
        }*/

                if (v1.vertex_label != v2.vertex_label)
                {
                    return false;
                }

                if (v1.itr_count != v2.itr_count)
                {
                    return false;
                }

                bool none = true; // at least one entry in aggregation input must be set to 1

                auto next_itr_count = v1.itr_count + 1;

                //assert(next_itr_count <= v1.max_itr_count);

                for (size_t i = 0; i < 8; i++)
                {
                    //for (size_t i = 0; i < 9; i++) {
                    if (aggregation_steps[next_itr_count][i] == (Boolean)1)
                    {
                        none = false;
                        if (v1.visited_vertices[i] != v2.visited_vertices[i])
                        {
                            return false;
                        }
                    }
                }

                if (none)
                {
                    std::cerr << "Error: wrong input." << std::endl;
                    return true; // output will be erroneous
                }

                return true;
            }
        };

        struct VisitorCompare
        {
        public:
            bool operator()(const visitor_type &v1, const visitor_type &v2) const
            {
                auto return_value = (v1.itr_count == v2.itr_count) &&
                                    (v1.target_vertex_label == v2.target_vertex_label) &&
                                    (v1.sequence_number == v2.sequence_number);
                return return_value;
            }
        };

        struct VisitorHash
        {
        public:
            size_t operator()(const visitor_type &visitor) const
            {
                return visitor.vertex_label;
                //return visitor.vertex_label + (uint64_t)MPI_Wtime();
                //std::chrono::time_point<std::chrono::system_clock> now =
                //  std::chrono::system_clock::now();
                //auto duration = now.time_since_epoch();
                //auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
                //return visitor.vertex_label + (uint64_t)microseconds;
                //return 0;
            }
        };

        typedef std::unordered_set<visitor_type, VisitorHash, VisitorCompare> VisitorSet;
        //VisitorSet* visitor_set_receive = new VisitorSet();
        //VisitorSet* visitor_set_send = new VisitorSet();
        //visitor_set_receive->clear();
        //visitor_set_send->clear();

        // using the vertex_data class for aggregation - one set per vertex
        typedef DelegateGraphVertexDataSTDAllocator<VisitorSet> VertexVisitorSetCollection;
        VertexVisitorSetCollection visitor_set_receive(*g);
        visitor_set_receive.clear();

        /////////////////////////////////////////////////////////////////////////////

        // batching

        // setup token sources
        std::unordered_set<Vertex> token_source_set;
        VertexUint8Map batch_token_source_map;

        for (auto vitr = g->vertices_begin();
             vitr != g->vertices_end(); vitr++)
        {
            vertex_locator vertex = *vitr;

            if (vertex_active[vertex] && vertex_metadata[vertex] == pattern[0])
            {
                BitSet vertex_template_vertices(template_vertices[vertex]); // template_vertices
                if (vertex_template_vertices.none() || !vertex_template_vertices.test(pattern_indices[0]))
                {
                    continue;
                }

                // nonunique metadata
                if (path_checking_filter_nlcc)
                {
                    // initiate token only from vertices with multiple template vertex matches
                    if (!expect_target_vertex)
                    { // path checking
                        if (!vertex_template_vertices.test(pattern_indices[0]) && vertex_template_vertices.count() == 1)
                        {
                            continue;
                        }
                    }
                }
                // nonunique metadata

                auto find_token_source = token_source_set.find(g->locator_to_label(vertex));
                if (find_token_source == token_source_set.end())
                {
                    auto insert_status = token_source_set.insert(g->locator_to_label(vertex));
                    if (!insert_status.second)
                    {
                        std::cerr << "Error: failed to add an element to the set." << std::endl;
                        //return false;
                    }
                }

            } // if

        } // for

        for (auto vitr = g->controller_begin();
             vitr != g->controller_end(); vitr++)
        {
            vertex_locator vertex = *vitr;

            if (vertex_active[vertex] && vertex_metadata[vertex] == pattern[0])
            {
                BitSet vertex_template_vertices(template_vertices[vertex]); // template_vertices
                if (vertex_template_vertices.none() || !vertex_template_vertices.test(pattern_indices[0]))
                {
                    continue;
                }

                // nonunique metadata
                if (path_checking_filter_nlcc)
                {
                    // initiate token only from vertices with multiple template vertex matches
                    if (!expect_target_vertex)
                    { // path checking
                        if (!vertex_template_vertices.test(pattern_indices[0]) && vertex_template_vertices.count() == 1)
                        {
                            continue;
                        }
                    }
                }
                // nonunique metadata

                auto find_token_source = token_source_set.find(g->locator_to_label(vertex));
                if (find_token_source == token_source_set.end())
                {
                    auto insert_status = token_source_set.insert(g->locator_to_label(vertex));
                    if (!insert_status.second)
                    {
                        std::cerr << "Error: failed to add an element to the set." << std::endl;
                        //return false;
                    }
                }

            } // if

        } // for

        MPI_Barrier(MPI_COMM_WORLD);

        size_t global_token_source_set_size =
            ///havoqgt::mpi::mpi_all_reduce(token_source_set.size(), std::plus<size_t>(),
            ///  MPI_COMM_WORLD);
            havoqgt::mpi_all_reduce(token_source_set.size(), std::plus<size_t>(),
                                    MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD); // TODO: do we need this?

        // batch parameters
        uint64_t max_batch_size = mpi_size;
        //uint64_t max_ranks_per_itr = mpi_size / 36; // quartz
        uint64_t max_ranks_per_itr = batch_size;

        uint64_t batch_count = 0;

        if (mpi_rank == 0) {
          std::cout << "Token Passing [" << pl << "] | Batch Size : "
            << max_ranks_per_itr << std::endl;
        }

         if (mpi_rank == 0)
         {
             std::cout << "Token Passing [" << pl << "] | Batch Size : "
                       << max_ranks_per_itr << " | Global Token Source Count : "
                       << global_token_source_set_size << std::endl;
         }

        // batch processing

        for (auto batch_mpi_rank = 0; batch_mpi_rank < max_batch_size;
             batch_mpi_rank += max_ranks_per_itr)
        {

            auto batch_max_mpi_rank = (batch_mpi_rank + max_ranks_per_itr - 1) <= (mpi_size - 1) ? (batch_mpi_rank + max_ranks_per_itr - 1) : (mpi_size - 1);

            double time_start = MPI_Wtime();

            batch_token_source_map.clear();
            assert(batch_mpi_rank + max_ranks_per_itr <= mpi_size);

            // setup batch token source map
            if (mpi_rank >= batch_mpi_rank && mpi_rank < (batch_mpi_rank + max_ranks_per_itr))
            {
                for (auto vitr = token_source_set.begin(); vitr != token_source_set.end();)
                {
                    auto find_token_source = batch_token_source_map.find(*vitr);
                    if (find_token_source == batch_token_source_map.end())
                    {
                        auto insert_status = batch_token_source_map.insert({*vitr, false});
                        if (!insert_status.second)
                        {
                            std::cerr << "Error: failed to add an element to the map." << std::endl;
                        }
                        vitr = token_source_set.erase(vitr); // C++11
                    }
                    else
                    {
                        std::cerr << "Error: unexpected item in the map." << std::endl;
                    }
                } // for
            }     // if

            // Test
            //if (batch_count <= 108) {
            //  batch_token_source_map.clear();
            //}

            MPI_Barrier(MPI_COMM_WORLD); // TODO: do we need this here?

            size_t global_batch_token_source_map_size = 1; // TODO: ?
            //  havoqgt::mpi::mpi_all_reduce(batch_token_source_map.size(),
            //  std::plus<size_t>(), MPI_COMM_WORLD);
            //MPI_Barrier(MPI_COMM_WORLD);

            if (global_batch_token_source_map_size == 0)
            { // skip distributed processing
                batch_count++;
                continue;
                MPI_Barrier(MPI_COMM_WORLD); // TODO: do we need this here?
            }

            if (mpi_rank == 0)
            {
                std::cout << "Token Passing [" << pl << "] | Batch #" << batch_count
                          << " | MPI Ranks : " << batch_mpi_rank << " - " << batch_max_mpi_rank
                          //<< (batch_mpi_rank + max_ranks_per_itr - 1)
                          //<< " | Global Batch Token Source Count : "
                          //<< global_batch_token_source_map_size
                          << " | Asynchronous Traversal ..." << std::endl;
            }

            ///////////////////////////////////////////////////////////////////////////

            // token passing

            //visitor_set_receive->clear();
            visitor_set_receive.clear();
            vertex_sequence_number.reset(0);
            
            bool stop = false;
            MPI_Barrier(MPI_COMM_WORLD);
            register_timer_nlcc(stop, PRUNING_TIME);
            

            //typedef tppm_visitor_nlcc<TGraph, Vertex, BitSet> visitor_type;
            auto alg_data = std::forward_as_tuple(vertex_metadata, pattern, pattern_indices, vertex_rank,
                                                  pattern_graph, vertex_state_map, batch_token_source_map, pattern_cycle_length, pattern_valid_cycle,
                                                  pattern_found,
                                                  edge_metadata, g, vertex_token_source_set, vertex_active, template_vertices, vertex_active_edges_map,
                                                  pattern_selected_vertices, paths_result_file, pattern_enumeration_indices, visitor_set_receive,
                                                  superstep_var, vertex_sequence_number, pattern_aggregation_steps, stop);

            auto vq = havoqgt::create_visitor_queue<visitor_type, havoqgt::detail::visitor_priority_queue>(g, alg_data);

            vq.init_visitor_traversal();
            MPI_Barrier(MPI_COMM_WORLD); // TODO: do we need this here?
            if(!stop){
                timer_delete(id_nlcc);
            }
            // Test
            // TODO: this is actually useful for analysis, uncomment
            // Test

            // token passing

            ///////////////////////////////////////////////////////////////////////////

            double time_end = MPI_Wtime();
            if (mpi_rank == 0)
            {
                std::cout << "Token Passing [" << pl << "] | Batch #" << batch_count
                          << " | MPI Ranks : " << batch_mpi_rank << " - " << batch_max_mpi_rank
                          //<< (batch_mpi_rank + max_ranks_per_itr - 1)
                          //<< " | Global Batch Token Source Count : "
                          //<< global_batch_token_source_map_size
                          << " | Time : " << time_end - time_start << std::endl;
            }

            // TODO: remove the invalid vertices here?

            // update the global token_source_map
            for (auto &s : batch_token_source_map)
            {
                auto find_token_source = token_source_map.find(s.first);
                if (find_token_source == token_source_map.end())
                {
                    auto insert_status = token_source_map.insert({s.first, s.second});
                    if (!insert_status.second)
                    {
                        std::cerr << "Error: failed to add an element to the map." << std::endl;
                    }
                }
                else
                {
                    std::cerr << "Error: unexpected item in the map." << std::endl;
                }
            } // for

            MPI_Barrier(MPI_COMM_WORLD); // TODO: do we need this here?

            batch_count++;
        } // for

        // batch processing

        // batching

        /////////////////////////////////////////////////////////////////////////////

        // Test
        MPI_Barrier(MPI_COMM_WORLD); // TODO: do we need this here?
    }

    ///////////////////////////////////////////////////////////////////////////////

#ifdef NO_BATCH
    template <typename TGraph, typename Vertex, typename VertexMetaData, typename PatternData,
              typename PatternIndices, typename PatternEnumerationIndices,
              typename VertexRank, typename PatternGraph,
              typename VertexStateMap, typename TokenSourceMap, typename EdgeMetaData,
              typename VertexSetCollection, typename VertexActive, typename TemplateVertex,
              typename VertexUint8MapCollection, typename BitSet>
    void token_passing_pattern_matching(TGraph *g, VertexMetaData &vertex_metadata,
                                        PatternData &pattern, PatternIndices &pattern_indices,
                                        PatternEnumerationIndices &pattern_enumeration_indices,
                                        VertexRank &vertex_rank, PatternGraph &pattern_graph, VertexStateMap &vertex_state_map,
                                        TokenSourceMap &token_source_map, size_t pattern_cycle_length, bool pattern_valid_cycle,
                                        std::vector<uint8_t>::reference pattern_found, EdgeMetaData &edge_metadata,
                                        VertexSetCollection &vertex_token_source_set, VertexActive &vertex_active,
                                        TemplateVertex &template_vertices, VertexUint8MapCollection &vertex_active_edges_map,
                                        bool pattern_selected_vertices, std::ofstream &paths_result_file,
                                        uint64_t &message_count)
    { // TODO: bool& pattern_found does not work, why?
        //std::cout << "token_passing_pattern_matching_new.hpp" << std::endl;

        typedef tppm_visitor_nlcc<TGraph, Vertex, BitSet> visitor_type;
        auto alg_data = std::forward_as_tuple(vertex_metadata, pattern, pattern_indices, vertex_rank,
                                              pattern_graph, vertex_state_map, token_source_map, pattern_cycle_length, pattern_valid_cycle, pattern_found,
                                              edge_metadata, g, vertex_token_source_set, vertex_active, template_vertices, vertex_active_edges_map,
                                              pattern_selected_vertices, paths_result_file, pattern_enumeration_indices);
        auto vq = havoqgt::create_visitor_queue<visitor_type, /*havoqgt::detail::visitor_priority_queue*/ tppm_queue>(g, alg_data);
        ///vq.init_visitor_traversal_new();
        //vq.init_visitor_traversal_new_batch();
        //vq.init_visitor_traversal_new_alt();
        vq.init_visitor_traversal();
        MPI_Barrier(MPI_COMM_WORLD);
    }
#endif

} ///} //end namespace havoqgt::mpi
