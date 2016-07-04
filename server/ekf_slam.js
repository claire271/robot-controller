//Nodejs support
module.exports = {
  ekf_slam: ekf_slam
};

//Implementation based on http://ocw.mit.edu/courses/aeronautics-and-astronautics/16-412j-cognitive-robotics-spring-2005/projects/1aslam_blas_repo.pdf

const conv = Math.PI / 180.0; //Conversion to radians
const MAXLANDMARKS = 3000;
const MAXERROR = 0.5; //If the landmark is within 20cm of another landmark, it is the same
const MINOBSERVATIONS = 15; //Observations required to be a landmark
const LIFE = 40;
const MAX_RANGE = 1;
const MAXTRIALS = 1000; //RANSAC: max times to run algorithm
const MAXSAMPLE = 10; //RANSAC: randomly select X points
const MINLINEPOINTS = 30; //RANSAC: if less than 40 points, stop algorithm (stop finding consensus)
const RANSAC_TOLERANCE = 0.05; //RANSAC: if point is within X distance of line, it is part of line
const RANSAC_CONSENSUS = 30; //RANSAC: at least 30 votes required to determine a line

const degreesPerScan = 0.5; //Probably going to change


function ekf_slam() {
  var landmarkDB = new Array(MAXLANDMARKS);
  var DBSize = 0;
  var IDtoID = new Array(MAXLANDMARKS);
  for(var i = 0;i < MAXLANDMARKS;i++) {
    IDtoID[i] = new Array(2);
  }
  var EKFLandmarks = 0;

  this.GetSlamID = function(id) {
    for(var i = 0;i < EKFLandmarks;i++) {
      if(IDtoID[i][0] === id)
        return IDtoID[i][1];
    }
    return -1;
  }

  this.GetSlamID = function(landmarkID, slamID) {
    IDtoID[EKFLandmarks][0] = landmarkID;
    IDtoID[EKFLandmarks][1] = slamID;
    EKFLandmarks++;
    return 0;
  }

  
}


function landmark() {
  var pos = [0.0,0.0]; //landmarks (x,y) position relative to map 
  var id = -1; //the landmarks unique ID 
  var life = LIFE; //a life counter used to determine whether to disca rd a landmark
  var totalTimesObserved = 0; //the number of times we have seen landmark 
  var range; //last observed range to landmark 
  var bearing; //last observed bearing to landmark 

  //RANSAC: Now store equation of a line
  var a = -1; 
  var b = -1; 

  var rangeError; //distance from robot position to the wall we are using as a landmark (to calculate error)
  var bearingError; //bearing from robot position to the wall we are using as a landmark (to calculate error)
}
