#ifndef FIELDLINE_H
#define FIELDLINE_H

#include "visionfieldobject.h"
#include "Tools/Math/Line.h"
#include "Tools/Math/General.h"

class FieldLine : public VisionFieldObject
{
public:
    FieldLine(const Line& line = Line());
    FieldLine(double rho, double phi);

    void set(double rho, double phi);
        
    //! @brief returns the field position relative to the robot.
    Vector3<float> getRelativeFieldCoords() const {return Vector3<float>(0,0,0);}
    /*!
      @brief pushes the object to the external field objects.
      @param fieldobjects a pointer to the global list of field objects.
      @param timestamp the image timestamp.
      @return the success of the operation.
      */
    bool addToExternalFieldObjects(FieldObjects* fieldobjects, float timestamp) const {return false;}
    //! @brief applies a series of checks to decide if the object is valid.
    bool check() const {return true;}

    Line getLineEquation() const;

    
    //! @brief Stream output for labelling purposes
    void printLabel(ostream& out) const {out << getVFOName(FIELDLINE) << " " << getShortLabel();}
    //! @brief Brief stream output for labelling purposes
    //void printLabelBrief(ostream& out) const {out << getVFOName(FIELDLINE) << " " << }
    Vector2<double> getShortLabel() const {return Vector2<double>(m_screen_line.getRho(), m_screen_line.getPhi());}

    //! @brief Calculation of error for optimisation - assumed measured = (rho, phi)
    double findError(const Vector2<double>& measured) const;
    double findError(const FieldLine& measured) const;

    void render(cv::Mat& mat) const;
    void render(cv::Mat& mat, cv::Scalar colour) const;

    //! @brief output stream operator
    friend ostream& operator<< (ostream& output, const FieldLine& l);
    //! @brief output stream operator for a vector of FieldLines
    friend ostream& operator<< (ostream& output, const vector<FieldLine>& v);

    Line m_screen_line,
         m_field_mapped_line;
//    double m_rho,
//           m_phi;
};

#endif // FIELDLINE_H