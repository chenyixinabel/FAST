// Copyright 2013-2020 Lawrence Livermore National Security, LLC and other
// HavoqGT Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <tuple>

#include <boost/algorithm/string.hpp>

//#include <havoqgt/approximate_pattern_matching/util.hpp>
#include <prunejuice/file_utilities.hpp>
#include <prunejuice/utilities.hpp>

using namespace prunejuice::utilities;

template <typename Vertex, typename Edge, typename VertexData, typename PatternGraph>
class pattern_nonlocal_constraint
{
public:
  pattern_nonlocal_constraint(PatternGraph &pattern_graph) : input_pattern_path_length(0),
                                                                                   input_pattern(0),
                                                                                   all_patterns(0),
                                                                                   one_path_patterns(0),
                                                                                   two_path_patterns(0),
                                                                                   enumeration_patterns(0),
                                                                                   aggregation_steps(0)
  {
    read_non_local_constraint(pattern_graph);
  }
  pattern_nonlocal_constraint(PatternGraph &pattern_graph,
                              std::string pattern_non_local_constraint_filename) : input_pattern_path_length(0),
                                                                                   input_pattern(0),
                                                                                   all_patterns(0),
                                                                                   one_path_patterns(0),
                                                                                   two_path_patterns(0),
                                                                                   enumeration_patterns(0),
                                                                                   aggregation_steps(0)
  {

    read_non_local_constraint(pattern_graph, pattern_non_local_constraint_filename);
  }

  pattern_nonlocal_constraint(PatternGraph &pattern_graph,
                              std::string pattern_non_local_constraint_filename,
                              std::string pattern_vertex_non_local_constraint_filename) : input_pattern_path_length(0),
                                                                                          input_pattern(0),
                                                                                          all_patterns(0),
                                                                                          one_path_patterns(0),
                                                                                          two_path_patterns(0),
                                                                                          enumeration_patterns(0),
                                                                                          aggregation_steps(0)
  {

    read_non_local_constraint(pattern_graph, pattern_non_local_constraint_filename);
    read_vertex_non_local_constraint(pattern_vertex_non_local_constraint_filename);
  }

  pattern_nonlocal_constraint(std::string pattern_input_filename,
                              std::string pattern_enumeration_input_filename, bool is_integral_type, bool is_enumeration) : input_pattern_path_length(0),
                                                                                                                            input_pattern(0),
                                                                                                                            all_patterns(0),
                                                                                                                            one_path_patterns(0),
                                                                                                                            two_path_patterns(0),
                                                                                                                            enumeration_patterns(0),
                                                                                                                            aggregation_steps(0),
                                                                                                                            join_vertices(0)
  {

    //std::cout << "Reading the input pattern list ..." << std::endl;
    read_pattern_list(pattern_input_filename, is_integral_type);
    if (is_enumeration)
    {
      //read_pattern_enumeration_list(pattern_enumeration_input_filename, is_integral_type);
      read_pattern_enumeration_list_2(pattern_enumeration_input_filename, is_integral_type);
    }
  }

  //~pattern_util() {}
  ~pattern_nonlocal_constraint() {}

  static void output_pattern(std::vector<VertexData> pattern)
  {
    for (auto p : pattern)
    {
      std::cout << std::to_string(p) << ", ";
    }
    std::cout << std::endl;
  }

  size_t input_pattern_path_length;
  std::vector<VertexData> input_pattern;
  std::vector<std::vector<VertexData>> all_patterns;
  std::vector<std::vector<VertexData>> one_path_patterns;
  std::vector<std::vector<VertexData>> two_path_patterns;

  std::vector<std::tuple<std::vector<VertexData>, std::vector<Vertex>, Edge, bool, bool, bool>> input_patterns;
  // TODO: verify stoull, bool, uint8_t compatibility
  std::vector<std::vector<Vertex>> enumeration_patterns;
  std::vector<std::vector<uint8_t>> aggregation_steps;

  std::vector<Vertex> join_vertices;

private:
  void read_non_local_constraint(PatternGraph &pattern_graph,
                                 std::string pattern_non_local_constraint_filename,
                                 bool is_integral_type = true)
  {
    std::ifstream pattern_non_local_constraint_file(pattern_non_local_constraint_filename, std::ifstream::in);
    if (is_file_empty(pattern_non_local_constraint_file))
    {
      //std::cout << pattern_non_local_constraint_filename << " is empty."
      //  << std::endl;
      return;
    }
    std::string line;
    while (std::getline(pattern_non_local_constraint_file, line))
    {
      //std::cout << line << std::endl; // Test
      boost::trim(line); // important

      std::istringstream iss(line);

      auto tokens = split(line, ':');
      assert(tokens.size() > 5); // TODO: improve

      boost::trim(tokens[0]); // important
      boost::trim(tokens[1]); // important
      boost::trim(tokens[2]); // important
      boost::trim(tokens[3]); // important
      boost::trim(tokens[4]); // important
      boost::trim(tokens[5]); // important

      auto vertices = split<Vertex>(tokens[0], ' ');
      assert(vertices.size() > 2);

      std::vector<VertexData> vertex_data(0);

      for (auto v = 0; v < vertices.size(); v++)
      {
        vertex_data.push_back(pattern_graph.vertex_data[vertices[v]]);
      }

      assert(vertices.size() == vertex_data.size());

      if (is_integral_type)
      {
        input_patterns.push_back(
            std::forward_as_tuple(
                vertex_data,
                vertices,
                (vertices.size() - 2),  // path length
                std::stoull(tokens[3]), // 0 - acyclic, 1 - cyclic
                std::stoull(tokens[4]), // 0 - regular, 1 - TDS
                std::stoull(tokens[5])  // 0 - skip LCC, 1 - invoke LCC
                ));

        enumeration_patterns.push_back(split<Vertex>(tokens[1], ' '));
        aggregation_steps.push_back(split<uint8_t>(tokens[2], ' ')); // TODO: verify type compatibility
      }
    }
    pattern_non_local_constraint_file.close();
  }

void read_non_local_constraint(PatternGraph &pattern_graph, bool is_integral_type = true)
  {
    for(auto line : pattern_graph.non_local_constraints){
      boost::trim(line); // important

      std::istringstream iss(line);

      auto tokens = split(line, ':');
      assert(tokens.size() > 5); // TODO: improve

      boost::trim(tokens[0]); // important
      boost::trim(tokens[1]); // important
      boost::trim(tokens[2]); // important
      boost::trim(tokens[3]); // important
      boost::trim(tokens[4]); // important
      boost::trim(tokens[5]); // important

      auto vertices = split<Vertex>(tokens[0], ' ');
      assert(vertices.size() > 2);

      std::vector<VertexData> vertex_data(0);

      for (auto v = 0; v < vertices.size(); v++)
      {
        vertex_data.push_back(pattern_graph.vertex_data[vertices[v]]);
      }

      assert(vertices.size() == vertex_data.size());

      if (is_integral_type)
      {
        input_patterns.push_back(
            std::forward_as_tuple(
                vertex_data,
                vertices,
                (vertices.size() - 2),  // path length
                std::stoull(tokens[3]), // 0 - acyclic, 1 - cyclic
                std::stoull(tokens[4]), // 0 - regular, 1 - TDS
                std::stoull(tokens[5])  // 0 - skip LCC, 1 - invoke LCC
                ));

        enumeration_patterns.push_back(split<Vertex>(tokens[1], ' '));
        aggregation_steps.push_back(split<uint8_t>(tokens[2], ' ')); // TODO: verify type compatibility
      }
    }
  }


  void read_vertex_non_local_constraint(std::string pattern_vertex_non_local_constraint_filename)
  {
    std::ifstream pattern_vertex_non_local_constraint_file(pattern_vertex_non_local_constraint_filename, std::ifstream::in);
    std::string line;
    while (std::getline(pattern_vertex_non_local_constraint_file, line))
    {
      std::istringstream iss(line);

      auto tokens_a = split(line, ':');
      if (tokens_a.size() < 2)
      {
        // vertex does not have a non-local constraint
        // TODO: ?
        continue;
      }

      for (size_t i = 0; i < tokens_a.size(); i++)
      {
        boost::trim(tokens_a[i]);

        if (i == 0)
        {
          // the vertex
        }

        if (i == 1)
        {

          auto tokens_b = split(tokens_a[i], ',');
          if (tokens_b.size() < 1)
          {
            // TODO: error?
            continue;
          }

          for (size_t j = 0; j < tokens_b.size(); j++)
          {
            // a key, value pair, key - non-local constraint ID,
            // value - is the non-local constraint mandatory for
            // the vertex type
            boost::trim(tokens_b[j]);
            auto tokens_c = split(tokens_b[j], ' ');
            assert(tokens_c.size() == 2);

            boost::trim(tokens_c[0]);
            boost::trim(tokens_c[1]);
          } // for
        }   // if
      }     // for
    }
    pattern_vertex_non_local_constraint_file.close();
  }

  void read_pattern_list(std::string pattern_input_filename, bool is_integral_type)
  {
    std::ifstream pattern_input_file(pattern_input_filename,
                                     std::ifstream::in);
    std::string line;
    while (std::getline(pattern_input_file, line))
    {
      std::istringstream iss(line);
      //std::cout << line << std::endl;

      auto tokens = split(line, ':');
      assert(tokens.size() > 1);

      boost::trim(tokens[0]); // important
      boost::trim(tokens[1]); // important
      boost::trim(tokens[2]); // important
      boost::trim(tokens[3]); // important
      boost::trim(tokens[4]); // important
      boost::trim(tokens[5]); // important

      if (is_integral_type)
      {
        input_patterns.push_back(
            std::forward_as_tuple(
                split<VertexData>(tokens[0], ' '), split<Vertex>(tokens[1], ' '),
                std::stoull(tokens[2]), std::stoull(tokens[3]),
                std::stoull(tokens[4]), std::stoull(tokens[5])));
      }
      else
      {
        input_patterns.push_back(
            std::forward_as_tuple(
                split_char<VertexData>(tokens[0], ' '), split<Vertex>(tokens[1], ' '),
                std::stoull(tokens[2]), std::stoull(tokens[3]),
                std::stoull(tokens[4]), std::stoull(tokens[5])));
      }
    }
    pattern_input_file.close();
  }

  void read_join_vertex_list(std::string join_vertex_input_filename,
                             bool is_integral_type)
  {
    std::ifstream join_vertex_input_file(join_vertex_input_filename,
                                         std::ifstream::in);
    std::string line;
    while (std::getline(join_vertex_input_file, line))
    {
      boost::trim(line); // important

      //auto tokens = split<Vertex>(line, ' ');
      //assert(tokens.size() > 0);

      if (is_integral_type)
      {
        join_vertices.push_back(std::stoull(line));
      }
    }
    join_vertex_input_file.close();
  }

  void read_pattern_enumeration_list(std::string pattern_enumeration_input_filename,
                                     bool is_integral_type)
  {
    std::ifstream pattern_enumeration_input_file(pattern_enumeration_input_filename,
                                                 std::ifstream::in);
    std::string line;
    while (std::getline(pattern_enumeration_input_file, line))
    {
      //std::cout << line << std::endl;
      boost::trim(line); // important

      std::istringstream iss(line);

      auto tokens = split(line, ':');
      assert(tokens.size() > 1);

      //boost::trim(tokens[0]); // important
      boost::trim(tokens[1]); // important

      if (is_integral_type)
      {
        enumeration_patterns.push_back(split<Vertex>(tokens[1], ' '));
      }
    }
    pattern_enumeration_input_file.close();
  }

  void read_pattern_enumeration_list_2(std::string pattern_enumeration_input_filename,
                                       bool is_integral_type)
  {
    std::ifstream pattern_enumeration_input_file(pattern_enumeration_input_filename,
                                                 std::ifstream::in);
    std::string line;
    while (std::getline(pattern_enumeration_input_file, line))
    {
      //std::cout << line << std::endl;
      boost::trim(line); // important

      std::istringstream iss(line);

      auto tokens = split(line, ':');
      assert(tokens.size() > 2);

      //boost::trim(tokens[0]); // important
      boost::trim(tokens[1]); // important
      boost::trim(tokens[2]); // important

      if (is_integral_type)
      {
        enumeration_patterns.push_back(split<Vertex>(tokens[1], ' '));
        aggregation_steps.push_back(split<uint8_t>(tokens[2], ' '));
      }
    }
    pattern_enumeration_input_file.close();
  }

  void read_pattern_enumeration_list_3(std::string pattern_enumeration_input_filename,
                                       bool is_integral_type)
  {
    std::ifstream pattern_enumeration_input_file(pattern_enumeration_input_filename,
                                                 std::ifstream::in);
    std::string line;
    while (std::getline(pattern_enumeration_input_file, line))
    {
      //std::cout << line << std::endl;
      boost::trim(line); // important

      std::istringstream iss(line);

      auto tokens = split(line, ':');
      assert(tokens.size() > 2);

      //boost::trim(tokens[0]); // important
      boost::trim(tokens[1]); // important
      boost::trim(tokens[2]); // important

      if (is_integral_type)
      {
        enumeration_patterns.push_back(split<Vertex>(tokens[1], ' '));
        //aggregation_steps.push_back(split<uint8_t>(tokens[2], ' '));
      }
    }
    pattern_enumeration_input_file.close();
  }

  void read_pattern_aggregation_list_3(std::string pattern_aggregation_input_filename,
                                       bool is_integral_type)
  {
    std::ifstream pattern_aggregation_input_file(pattern_aggregation_input_filename,
                                                 std::ifstream::in);
    std::string line;
    while (std::getline(pattern_aggregation_input_file, line))
    {
      //std::cout << line << std::endl;
      boost::trim(line); // important

      std::istringstream iss(line);

      if (is_integral_type)
      {
        aggregation_steps.push_back(split<uint8_t>(line, ' '));
      }
    }
    pattern_aggregation_input_file.close();
  }

  void read_pattern_input(std::string pattern_input_filename)
  {
    std::ifstream pattern_input_file(pattern_input_filename,
                                     std::ifstream::in);
    std::string line;
    while (std::getline(pattern_input_file, line))
    {
      std::istringstream iss(line);
      std::cout << line << std::endl;
      VertexData p(0);
      iss >> p;
      input_pattern.push_back(p);
    }
    pattern_input_file.close();
  }

  void generate_pattern_combinations()
  {
    size_t max_path_length = input_pattern.size() - 1;
    size_t path_length = 1;
    while (path_length <= max_path_length)
    {
      for (size_t i = 0, j = i + path_length; j < input_pattern.size(); i++, j++)
      {
        std::vector<VertexData> pattern(input_pattern.begin() + i, input_pattern.begin() + j + 1);
        all_patterns.push_back(pattern);
        if (path_length == 1)
        {
          one_path_patterns.push_back(pattern);
        }
        else if (path_length == 2)
        {
          two_path_patterns.push_back(pattern);
        }
      }
      path_length++;
    }
  }

  void output_pattern_combinations()
  {
    for (auto pattern : all_patterns)
    {
      for (auto p : pattern)
      {
        std::cout << p << ", ";
      }
      std::cout << std::endl;
    }
  }
};
