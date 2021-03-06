/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h>
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

#define EPS 0.00001

using namespace std;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of
	//   x, y, theta and their uncertainties from GPS) and all weights to 1.
	// Add random Gaussian noise to each particle.
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).
	if (is_initialized) return;

	// Initialise the number of particles
	num_particles = 250;

	// Extracting standard deviations
  double std_x = std[0];
  double std_y = std[1];
  double std_theta = std[2];

	// Creating normal distributions
  normal_distribution<double> dist_x(x, std_x);
  normal_distribution<double> dist_y(y, std_y);
  normal_distribution<double> dist_theta(theta, std_theta);

	// Generate particles with normal distribution with mean on GPS values.
  for (int i = 0; i < num_particles; i++)
	{
		Particle particle;
		particle.id = i;
		particle.x = dist_x(gen);
		particle.y = dist_y(gen);
		particle.theta = dist_theta(gen);
		particle.weight = 1.0;
		particles.push_back(particle);
	}

	// The filter is now initialized.
  is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/

	// Extracting standard deviations
  double std_x = std_pos[0];
  double std_y = std_pos[1];
  double std_theta = std_pos[2];

	// Creating normal distributions
  normal_distribution<double> dist_x(0, std_x);
  normal_distribution<double> dist_y(0, std_y);
  normal_distribution<double> dist_theta(0, std_theta);

	// Calculate new state
	for (int i = 0; i < num_particles; i++)
	{
		double theta = particles[i].theta;

		// When there is no change in yaw
		if (fabs(yaw_rate) < EPS)
		{
			particles[i].x += velocity * delta_t * cos(theta);
			particles[i].y += velocity * delta_t * sin(theta);
		}
		else
		{
			particles[i].x += velocity / yaw_rate * (sin(theta + yaw_rate * delta_t) - sin(theta));
			particles[i].y += velocity / yaw_rate * (cos(theta) - cos(theta + yaw_rate * delta_t));
			particles[i].theta += yaw_rate * delta_t;
		}

		// Adding noise
		particles[i].x += dist_x(gen);
		particles[i].y += dist_y(gen);
		particles[i].theta += dist_theta(gen);
	}
}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to
	//   implement this method and use it as a helper during the updateWeights phase.

	// Get number of observations and predictions
	unsigned int num_observations = observations.size();
  unsigned int num_predictions = predicted.size();

	// For each observation
	for (unsigned int i = 0; i < num_observations; i++)
	{
		// Initialise min distance as a really big number.
    double minDistance = numeric_limits<double>::max();

		// Initialize the found map in something not possible.
    int mapId = -1;

		// For each prediction
		for (unsigned int j = 0; j < num_predictions; j++)
		{
			double xDistance = observations[i].x - predicted[j].x;
      double yDistance = observations[i].y - predicted[j].y;

      double distance = xDistance * xDistance + yDistance * yDistance;

      // If the "distance" is less than min, stored the id and update min.
      if ( distance < minDistance )
			{
        minDistance = distance;
        mapId = predicted[j].id;
      }
		}

		// Update the observation identifier.
    observations[i].id = mapId;
	}
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[],
		const std::vector<LandmarkObs> &observations, const Map &map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html

	double std_landmark_range = std_landmark[0];
  double std_landmark_bearing = std_landmark[1];

	for (int i = 0; i < num_particles; i++)
	{
		double cur_particle_x = particles[i].x;
		double cur_particle_y = particles[i].y;
		double theta = particles[i].theta;
		// Find landmarks in particle's range
		double sensor_range_sqrt = pow(sensor_range, 2);
		vector<LandmarkObs> in_range_landmarks;
		for (unsigned int j = 0; j < map_landmarks.landmark_list.size(); j++)
		{
			float landmark_x = map_landmarks.landmark_list[j].x_f;
			float landmark_y = map_landmarks.landmark_list[j].y_f;
			int landmark_id = map_landmarks.landmark_list[j].id_i;

			double d_x = cur_particle_x - landmark_x;
			double d_y = cur_particle_y - landmark_y;
			double temp = pow(d_x, 2) + pow(d_y, 2);
			if (temp <= sensor_range_sqrt)
			{
				in_range_landmarks.push_back(LandmarkObs{landmark_id, landmark_x, landmark_y});
			}
		}

		// Transforms observation coordinates
		vector<LandmarkObs> mapped_observations;
		for (unsigned int j = 0; j < observations.size(); j++)
		{
			double trans_x = cos(theta) * observations[j].x - sin(theta) * observations[j].y + cur_particle_x;
			double trans_y = sin(theta) * observations[j].x + cos(theta) * observations[j].y + cur_particle_y;
			mapped_observations.push_back(LandmarkObs{observations[j].id, trans_x, trans_y});
		}

		// Observation association to landmark
		dataAssociation(in_range_landmarks, mapped_observations);

		// Reseting weight
		particles[i].weight = 1.0;
		// Calculate weights
		for (unsigned int j = 0; j < mapped_observations.size(); j++)
		{
			double observation_x = mapped_observations[j].x;
			double observation_y = mapped_observations[j].y;
			int landmark_id = mapped_observations[j].id;

			double landmark_x, landmark_y;

			for (unsigned int k = 0; k < in_range_landmarks.size(); k++)
			{
				if (in_range_landmarks[k].id == landmark_id)
				{
					landmark_x = in_range_landmarks[k].x;
					landmark_y = in_range_landmarks[k].y;
					break;
				}
			}

			// Calculating weight
			double d_x = observation_x - landmark_x;
			double d_y = observation_y - landmark_y;

			double weight = (1/(2 * M_PI * std_landmark_range * std_landmark_bearing)) * exp(-(pow(d_x, 2)/(2 * pow(std_landmark_range, 2)) + pow(d_y, 2)/(2 * pow(std_landmark_bearing, 2))));
			if (weight == 0)
			{
				particles[i].weight *= EPS;
			}
			else
			{
				particles[i].weight *= weight;
			}
		}
 	}
}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight.
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution

	// Get weights
	weights.clear();
	for (int i = 0; i < num_particles; i++)
	{
		weights.push_back(particles[i].weight);
	}

	std::discrete_distribution<int> d(weights.begin(), weights.end());
	std::vector<Particle> weighted_sample(num_particles);

	for(int i = 0; i < num_particles; ++i)
	{
		int j = d(gen);
		weighted_sample.at(i) = particles.at(j);
	}

	particles = weighted_sample;
}

Particle ParticleFilter::SetAssociations(Particle& particle, const std::vector<int>& associations,
                                     const std::vector<double>& sense_x, const std::vector<double>& sense_y)
{
    //particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
    // associations: The landmark id that goes along with each listed association
    // sense_x: the associations x mapping already converted to world coordinates
    // sense_y: the associations y mapping already converted to world coordinates

    particle.associations = associations;
    particle.sense_x = sense_x;
    particle.sense_y = sense_y;

		return particle;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
