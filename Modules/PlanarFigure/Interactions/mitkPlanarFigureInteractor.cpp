/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/


#define PLANARFIGUREINTERACTOR_DBG MITK_DEBUG("PlanarFigureInteractor") << __LINE__ << ": "

#include "mitkPlanarFigureInteractor.h"
#include "mitkPointOperation.h"
#include "mitkPositionEvent.h"
#include "mitkPlanarFigure.h"
#include "mitkPlanarPolygon.h"
#include "mitkStatusBar.h"
#include "mitkDataNode.h"
#include "mitkInteractionConst.h"
#include "mitkAction.h"
#include "mitkStateEvent.h"
#include "mitkOperationEvent.h"
#include "mitkUndoController.h"
#include "mitkStateMachineFactory.h"
#include "mitkStateTransitionOperation.h"
#include "mitkBaseRenderer.h"
#include "mitkRenderingManager.h"
#include "mitkNodePredicateDataType.h"
#include "mitkNodePredicateOr.h"


//how precise must the user pick the point
//default value
mitk::PlanarFigureInteractor
::PlanarFigureInteractor(const char * type, DataNode* dataNode, int /* n */ )
: Interactor( type, dataNode ),
m_Precision( 6.5 ),
m_MinimumPointDistance( 25.0 ),
m_IsHovering( false ),
m_LastPointWasValid( false )
{
}

mitk::PlanarFigureInteractor::~PlanarFigureInteractor()
{
}

void mitk::PlanarFigureInteractor::SetPrecision( mitk::ScalarType precision )
{
  m_Precision = precision;
}


void mitk::PlanarFigureInteractor::SetMinimumPointDistance( ScalarType minimumDistance )
{
  m_MinimumPointDistance = minimumDistance;
}


// Overwritten since this class can handle it better!
float mitk::PlanarFigureInteractor
::CanHandleEvent(StateEvent const* stateEvent) const
{
  // If it is a key event that can be handled in the current state,
  // then return 0.5
  mitk::DisplayPositionEvent const *disPosEvent =
    dynamic_cast <const mitk::DisplayPositionEvent *> (stateEvent->GetEvent());

  const mitk::PositionEvent *positionEvent = dynamic_cast< const mitk::PositionEvent * > ( stateEvent->GetEvent() );
  if ( positionEvent == NULL )
  {
    return 0.0;
  }


  // Key event handling:
  if (disPosEvent == NULL)
  {
    // Check if the current state has a transition waiting for that key event.
    if (this->GetCurrentState()->GetTransition(stateEvent->GetId())!=NULL)
    {
      return 0.5;
    }
    else
    {
      return 0.0;
    }
  }

  mitk::PlanarFigure *planarFigure = dynamic_cast<mitk::PlanarFigure *>(
    m_DataNode->GetData() );

  if ( planarFigure != NULL )
  {
    bool selected = false;
    m_DataNode->GetBoolProperty("selected", selected);

    mitk::Point3D worldPoint3D = positionEvent->GetWorldPosition();

    mitk::Geometry2D *planarFigureGeometry2D =
      dynamic_cast< Geometry2D * >( planarFigure->GetGeometry( 0 ) );

    double planeThickness = planarFigureGeometry2D->GetExtentInMM( 2 );
    if ( planarFigure->IsPlaced()
      && planarFigureGeometry2D->Distance( worldPoint3D ) > planeThickness )
    {
      // don't react, when interaction is too far away
      return 0.0;
    }

    if ( planarFigure->IsPlaced() && selected )
    {
      // if a figure is placed, it has to return a higher value than one
      // that is not, even if the new one is already 'selected'
      return 0.75;
    }
    else if ( planarFigure->IsPlaced() )
    {
      return 0.7;
    }
    else if ( selected )
    {
      return 0.6;
    }
    // else fall through
  }
  return 0.42;
}


bool mitk::PlanarFigureInteractor
::ExecuteAction( Action *action, mitk::StateEvent const *stateEvent )
{
  bool ok = false;

  // Check corresponding data; has to be sub-class of mitk::PlanarFigure
  mitk::PlanarFigure *planarFigure =
    dynamic_cast< mitk::PlanarFigure * >( m_DataNode->GetData() );

  if ( planarFigure == NULL )
  {
    return false;
  }

  // Get the timestep to also support 3D+t
  const mitk::Event *theEvent = stateEvent->GetEvent();
  int timeStep = 0;
  //mitk::ScalarType timeInMS = 0.0;

  if ( theEvent )
  {
    if (theEvent->GetSender() != NULL)
    {
      timeStep = theEvent->GetSender()->GetTimeStep( planarFigure );
      //timeInMS = theEvent->GetSender()->GetTime();
    }
  }

  // Get Geometry2D of PlanarFigure
  mitk::Geometry2D *planarFigureGeometry =
    dynamic_cast< mitk::Geometry2D * >( planarFigure->GetGeometry( timeStep ) );

  // Get the Geometry2D of the window the user interacts with (for 2D point
  // projection)
  mitk::BaseRenderer *renderer = NULL;
  const Geometry2D *projectionPlane = NULL;
  if ( theEvent )
  {
    renderer = theEvent->GetSender();
    projectionPlane = renderer->GetCurrentWorldGeometry2D();
  }

  // TODO: Check if display and PlanarFigure geometries are parallel (if they are PlaneGeometries)



  switch (action->GetActionId())
  {
  case AcDONOTHING:
    PLANARFIGUREINTERACTOR_DBG << "AcDONOTHING";
    ok = true;
    break;

  case AcCHECKOBJECT:
    {
      PLANARFIGUREINTERACTOR_DBG << "AcCHECKOBJECT";
      if ( planarFigure->IsPlaced() )
      {
        this->HandleEvent( new mitk::StateEvent( EIDYES, NULL ) );
      }
      else
      {
        this->HandleEvent( new mitk::StateEvent( EIDNO, NULL ) );
      }
      ok = false;
      break;
    }

  case AcADD:
    {
      PLANARFIGUREINTERACTOR_DBG << "AcADD";
      // Invoke event to notify listeners that placement of this PF starts now
      planarFigure->InvokeEvent( StartPlacementPlanarFigureEvent() );

      // Use Geometry2D of the renderer clicked on for this PlanarFigure
      mitk::PlaneGeometry *planeGeometry = const_cast< mitk::PlaneGeometry * >(
        dynamic_cast< const mitk::PlaneGeometry * >(
        renderer->GetSliceNavigationController()->GetCurrentPlaneGeometry() ) );
      if ( planeGeometry != NULL )
      {
        planarFigureGeometry = planeGeometry;
        planarFigure->SetGeometry2D( planeGeometry );
      }
      else
      {
        ok = false;
        break;
      }

      // Extract point in 2D world coordinates (relative to Geometry2D of
      // PlanarFigure)
      Point2D point2D;
      if ( !this->TransformPositionEventToPoint2D( stateEvent, point2D,
        planarFigureGeometry ) )
      {
        ok = false;
        break;
      }

      // Place PlanarFigure at this point
      planarFigure->PlaceFigure( point2D );

      // Re-evaluate features
      planarFigure->EvaluateFeatures();
      //this->LogPrintPlanarFigureQuantities( planarFigure );

      // Set a bool property indicating that the figure has been placed in
      // the current RenderWindow. This is required so that the same render
      // window can be re-aligned to the Geometry2D of the PlanarFigure later
      // on in an application.
      m_DataNode->SetBoolProperty( "PlanarFigureInitializedWindow", true, renderer );

      // Update rendered scene
      renderer->GetRenderingManager()->RequestUpdateAll();

      ok = true;
      break;
    }

  case AcMOVEPOINT:
    {
      PLANARFIGUREINTERACTOR_DBG << "AcMOVEPOINT";
      bool isEditable = true;
      m_DataNode->GetBoolProperty( "planarfigure.iseditable", isEditable );

      // Extract point in 2D world coordinates (relative to Geometry2D of
      // PlanarFigure)
      Point2D point2D;
      if ( !this->TransformPositionEventToPoint2D( stateEvent, point2D,
        planarFigureGeometry ) || !isEditable )
      {
        ok = false;
        break;
      }

      // check if the control points shall be hidden during interaction
      bool hidecontrolpointsduringinteraction = false;
      m_DataNode->GetBoolProperty( "planarfigure.hidecontrolpointsduringinteraction", hidecontrolpointsduringinteraction );

      // hide the control points if necessary
      m_DataNode->SetBoolProperty( "planarfigure.drawcontrolpoints", !hidecontrolpointsduringinteraction );

      // Move current control point to this point
      planarFigure->SetCurrentControlPoint( point2D );

      // Re-evaluate features
      planarFigure->EvaluateFeatures();
      //this->LogPrintPlanarFigureQuantities( planarFigure );

      // Update rendered scene
      renderer->GetRenderingManager()->RequestUpdateAll();

      ok = true;
      break;
    }


  case AcCHECKNMINUS1:
    {
      PLANARFIGUREINTERACTOR_DBG << "AcCHECKNMINUS1";

      if ( planarFigure->GetNumberOfControlPoints() >=
        planarFigure->GetMaximumNumberOfControlPoints() )
      {
        // Initial placement finished: deselect control point and send an
        // event to notify application listeners
        planarFigure->Modified();
        planarFigure->DeselectControlPoint();
        planarFigure->InvokeEvent( EndPlacementPlanarFigureEvent() );
        planarFigure->InvokeEvent( EndInteractionPlanarFigureEvent() );
        planarFigure->SetProperty( "initiallyplaced", mitk::BoolProperty::New( true ) );
        m_DataNode->SetBoolProperty( "planarfigure.drawcontrolpoints", true );
        m_DataNode->Modified();
        this->HandleEvent( new mitk::StateEvent( EIDYES, stateEvent->GetEvent() ) );
      }
      else
      {
        this->HandleEvent( new mitk::StateEvent( EIDNO, stateEvent->GetEvent() ) );
      }

      // Update rendered scene
      renderer->GetRenderingManager()->RequestUpdateAll();

      ok = true;
      break;
    }


  case AcCHECKEQUALS1:
    {
      PLANARFIGUREINTERACTOR_DBG << "AcCHECKEQUALS1";

      // NOTE: Action name is a bit misleading; this action checks whether
      // the figure has already the minimum number of required points to
      // be finished (by double-click)

      const mitk::PositionEvent *positionEvent =
        dynamic_cast< const mitk::PositionEvent * > ( stateEvent->GetEvent() );
      if ( positionEvent == NULL )
      {
        ok = false;
        break;
      }

      if ( planarFigure->GetNumberOfControlPoints() > planarFigure->GetMinimumNumberOfControlPoints()  )
      {
        // Initial placement finished: deselect control point and send an
        // event to notify application listeners
        planarFigure->Modified();
        planarFigure->DeselectControlPoint();
        planarFigure->RemoveLastControlPoint();
        planarFigure->SetProperty( "initiallyplaced", mitk::BoolProperty::New( true ) );
        m_DataNode->SetBoolProperty( "planarfigure.drawcontrolpoints", true );
        m_DataNode->Modified();
        planarFigure->InvokeEvent( EndPlacementPlanarFigureEvent() );
        planarFigure->InvokeEvent( EndInteractionPlanarFigureEvent() );

        this->HandleEvent( new mitk::StateEvent( EIDYES, NULL ) );
      }
      else
      {
        this->HandleEvent( new mitk::StateEvent( EIDNO, NULL ) );
      }

      // Update rendered scene
      renderer->GetRenderingManager()->RequestUpdateAll();

      ok = true;
      break;
    }


  case AcCHECKPOINT:
    {
      PLANARFIGUREINTERACTOR_DBG << "AcCHECKPOINT";
      // Check if the distance of the current point to the previously set point in display coordinates
      // is sufficient (if a previous point exists)

      // Extract display position
      const mitk::PositionEvent *positionEvent =
        dynamic_cast< const mitk::PositionEvent * > ( stateEvent->GetEvent() );
      if ( positionEvent == NULL )
      {
        ok = false;
        break;
      }

      m_LastPointWasValid = IsMousePositionAcceptableAsNewControlPoint( stateEvent, planarFigure );
      if (m_LastPointWasValid)
      {
        this->HandleEvent( new mitk::StateEvent( EIDYES, stateEvent->GetEvent() ) );
      }
      else
      {
        this->HandleEvent( new mitk::StateEvent( EIDNO, stateEvent->GetEvent() ) );
      }

      ok = true;
      break;
    }


  case AcADDPOINT:
    {
      PLANARFIGUREINTERACTOR_DBG << "AcADDPOINT";
      bool selected = false;
      bool isEditable = true;
      m_DataNode->GetBoolProperty("selected", selected);
      m_DataNode->GetBoolProperty( "planarfigure.iseditable", isEditable );

      if ( !selected || !isEditable )
      {
        ok = false;
        break;
      }

      // If the planarFigure already has reached the maximum number
      if ( planarFigure->GetNumberOfControlPoints() >= planarFigure->GetMaximumNumberOfControlPoints() )
      {
        ok = false;
        break;
      }

      // Extract point in 2D world coordinates (relative to Geometry2D of
      // PlanarFigure)
      Point2D point2D, projectedPoint;
      if ( !this->TransformPositionEventToPoint2D( stateEvent, point2D,
        planarFigureGeometry ) )
      {
        ok = false;
        break;
      }

      // TODO: check segment of polyline we clicked in
      int nextIndex = -1;

      // We only need to check which position to insert the control point
      // when interacting with a PlanarPolygon. For all other types
      // new control points will always be appended

      /*
       * Added check for "initiallyplaced" due to bug 13097:
       *
       * There are two possible cases in which a point can be inserted into a PlanarPolygon:
       *
       * 1. The figure is currently drawn -> the point will be appended at the end of the figure
       * 2. A point is inserted at a userdefined position after the initial placement of the figure is finished
       *
       * In the second case we need to determine the proper insertion index. In the first case the index always has
       * to be -1 so that the point is appended to the end.
       *
       * These changes are neccessary because of a mac os x specific issue: If a users draws a PlanarPolygon then the
       * next point to be added moves according to the mouse position. If then the user left clicks in order to add
       * a point one would assume the last move position is identical to the left click position. This is actually the
       * case for windows and linux but somehow NOT for mac. Because of the insertion logic of a new point in the
       * PlanarFigure then for mac the wrong current selected point is determined.
       *
       * With this check here this problem can be avoided. However a redesign of the insertion logic should be considered
       */
      bool isFigureFinished = false;
      planarFigure->GetPropertyList()->GetBoolProperty( "initiallyplaced", isFigureFinished );

      if ( dynamic_cast<mitk::PlanarPolygon*>( planarFigure ) && isFigureFinished)
      {
        nextIndex = this->IsPositionOverFigure(
          stateEvent, planarFigure,
          planarFigureGeometry,
          projectionPlane,
          renderer->GetDisplayGeometry(),
          projectedPoint
          );
      }


      // Add point as new control point
      renderer->GetDisplayGeometry()->DisplayToWorld( projectedPoint, projectedPoint );

      if ( planarFigure->IsPreviewControlPointVisible() )
      {
        point2D = planarFigure->GetPreviewControlPoint();
      }

      planarFigure->AddControlPoint( point2D, nextIndex );

      if ( planarFigure->IsPreviewControlPointVisible() )
      {
        planarFigure->SelectControlPoint( nextIndex );
        planarFigure->ResetPreviewContolPoint();
      }

      // Re-evaluate features
      planarFigure->EvaluateFeatures();
      //this->LogPrintPlanarFigureQuantities( planarFigure );

      // Update rendered scene
      renderer->GetRenderingManager()->RequestUpdateAll();

      ok = true;
      break;
    }


  case AcDESELECTPOINT:
    {
      PLANARFIGUREINTERACTOR_DBG << "AcDESELECTPOINT";
      bool wasSelected = planarFigure->DeselectControlPoint();

      if ( wasSelected )
      {
        // Issue event so that listeners may update themselves
        planarFigure->Modified();
        planarFigure->InvokeEvent( EndInteractionPlanarFigureEvent() );

        m_DataNode->SetBoolProperty( "planarfigure.drawcontrolpoints", true );
        m_DataNode->SetBoolProperty( "planarfigure.ishovering", false );
        m_DataNode->Modified();

      }
      // falls through
      break;
    }

  case AcCHECKHOVERING:
    {
      PLANARFIGUREINTERACTOR_DBG << "AcCHECKHOVERING";
      mitk::Point2D pointProjectedOntoLine;
      int previousControlPoint = mitk::PlanarFigureInteractor::IsPositionOverFigure(
        stateEvent, planarFigure,
        planarFigureGeometry,
        projectionPlane,
        renderer->GetDisplayGeometry(),
        pointProjectedOntoLine
        );
      bool isHovering = ( previousControlPoint != -1 );

      int pointIndex = mitk::PlanarFigureInteractor::IsPositionInsideMarker(
        stateEvent, planarFigure,
        planarFigureGeometry,
        projectionPlane,
        renderer->GetDisplayGeometry() );

      int initiallySelectedControlPoint = planarFigure->GetSelectedControlPoint();

      if ( pointIndex >= 0 )
      {
        // If mouse is above control point, mark it as selected
        planarFigure->SelectControlPoint( pointIndex );

        // If mouse is hovering above a marker, it is also hovering above the figure
        isHovering = true;
      }
      else
      {
        // Mouse in not above control point --> deselect point
        planarFigure->DeselectControlPoint();
      }

      bool renderingUpdateNeeded = true;
      if ( isHovering )
      {
        if ( !m_IsHovering )
        {
          // Invoke hover event once when the mouse is entering the figure area
          m_IsHovering = true;
          planarFigure->InvokeEvent( StartHoverPlanarFigureEvent() );

          // Set bool property to indicate that planar figure is currently in "hovering" mode
          m_DataNode->SetBoolProperty( "planarfigure.ishovering", true );

          renderingUpdateNeeded = true;
        }

        bool selected = false;
        bool isExtendable = false;
        bool isEditable = true;
        m_DataNode->GetBoolProperty("selected", selected);
        m_DataNode->GetBoolProperty("planarfigure.isextendable", isExtendable);
        m_DataNode->GetBoolProperty( "planarfigure.iseditable", isEditable );

        if ( selected && isHovering && isExtendable && pointIndex == -1 && isEditable )
        {
          const mitk::PositionEvent *positionEvent =
            dynamic_cast< const mitk::PositionEvent * > ( stateEvent->GetEvent() );
          if ( positionEvent != NULL )
          {
            renderer->GetDisplayGeometry()->DisplayToWorld( pointProjectedOntoLine, pointProjectedOntoLine );
            planarFigure->SetPreviewControlPoint( pointProjectedOntoLine );

            renderingUpdateNeeded = true;
          }
        }
        else
        {
          planarFigure->ResetPreviewContolPoint();
        }

        if ( planarFigure->GetSelectedControlPoint() != initiallySelectedControlPoint  )
        {
          // the selected control point has changed -> rendering update necessary
          renderingUpdateNeeded = true;
        }

        this->HandleEvent( new mitk::StateEvent( EIDYES, stateEvent->GetEvent() ) );

        // Return true: only this interactor is eligible to react on this event
        ok = true;
      }
      else
      {
        if ( m_IsHovering )
        {
          planarFigure->ResetPreviewContolPoint();

          // Invoke end-hover event once the mouse is exiting the figure area
          m_IsHovering = false;
          planarFigure->InvokeEvent( EndHoverPlanarFigureEvent() );

          // Set bool property to indicate that planar figure is no longer in "hovering" mode
          m_DataNode->SetBoolProperty( "planarfigure.ishovering", false );

          renderingUpdateNeeded = true;
        }

        this->HandleEvent( new mitk::StateEvent( EIDNO, NULL ) );

        // Return false so that other (PlanarFigure) Interactors may react on this
        // event as well
        ok = false;
      }

      // Update rendered scene if necessray
      if ( renderingUpdateNeeded )
      {
        renderer->GetRenderingManager()->RequestUpdateAll();
      }
      break;
    }

  case AcCHECKSELECTED:
    {
      PLANARFIGUREINTERACTOR_DBG << "AcCHECKSELECTED";
      bool selected = false;
      m_DataNode->GetBoolProperty("selected", selected);

      if ( selected )
      {
        this->HandleEvent( new mitk::StateEvent( EIDYES, stateEvent->GetEvent() ) );
      }
      else
      {
        // Invoke event to notify listeners that this planar figure should be selected
        planarFigure->InvokeEvent( SelectPlanarFigureEvent() );
        this->HandleEvent( new mitk::StateEvent( EIDNO, NULL ) );
      }
    }

  case AcSELECTPICKEDOBJECT:
    {
      PLANARFIGUREINTERACTOR_DBG << "AcSELECTPICKEDOBJECT";
      //// Invoke event to notify listeners that this planar figure should be selected
      //planarFigure->InvokeEvent( SelectPlanarFigureEvent() );
      //planarFigure->InvokeEvent( StartInteractionPlanarFigureEvent() );

      // Check if planar figure is marked as "editable"
      bool isEditable = true;
      m_DataNode->GetBoolProperty( "planarfigure.iseditable", isEditable );

      int pointIndex = -1;

      if ( isEditable )
      {
        // If planar figure is editable, check if mouse is over a control point
        pointIndex = mitk::PlanarFigureInteractor::IsPositionInsideMarker(
          stateEvent, planarFigure,
          planarFigureGeometry,
          projectionPlane,
          renderer->GetDisplayGeometry() );
      }

      // If editing is enabled and the mouse is currently over a control point, select it
      if ( pointIndex >= 0 )
      {
        this->HandleEvent( new mitk::StateEvent( EIDYES, stateEvent->GetEvent() ) );

        // Return true: only this interactor is eligible to react on this event
        ok = true;
      }
      else
      {
        // we're not hovering above a control point -> deselect!
        planarFigure->DeselectControlPoint();

        this->HandleEvent( new mitk::StateEvent( EIDNO, stateEvent->GetEvent() ) );

        // Return false so that other (PlanarFigure) Interactors may react on this
        // event as well
        ok = false;
      }

      ok = true;
      break;
    }

  case AcENTEROBJECT:
    {
      PLANARFIGUREINTERACTOR_DBG << "AcENTEROBJECT";
      bool selected = false;
      m_DataNode->GetBoolProperty("selected", selected);

      // no need to invoke this if the figure is already selected
      if ( !selected )
      {
        planarFigure->InvokeEvent( SelectPlanarFigureEvent() );
      }

      planarFigure->InvokeEvent( ContextMenuPlanarFigureEvent() );
      ok = true;

      // we HAVE TO proceed with 'EIDNO' here to ensure correct states
      // and convenient application behaviour
      this->HandleEvent( new mitk::StateEvent( EIDNO, stateEvent->GetEvent() ) );
      break;
    }


  case AcSELECTPOINT:
    {
      PLANARFIGUREINTERACTOR_DBG << "AcSELECTPOINT";
      // Invoke event to notify listeners that interaction with this PF starts now
      planarFigure->InvokeEvent( StartInteractionPlanarFigureEvent() );

      // Reset the PlanarFigure if required
      if ( planarFigure->ResetOnPointSelect() )
      {
        this->HandleEvent( new mitk::StateEvent( EIDYES, stateEvent->GetEvent() ) );
      }
      else
      {
        this->HandleEvent( new mitk::StateEvent( EIDNO, stateEvent->GetEvent() ) );
      }

      ok = true;
      break;
    }

  case AcREMOVEPOINT:
    {
      PLANARFIGUREINTERACTOR_DBG << "AcREMOVEPOINT";
      bool isExtendable = false;
      m_DataNode->GetBoolProperty("planarfigure.isextendable", isExtendable);

      if ( isExtendable )
      {
        int selectedControlPoint = planarFigure->GetSelectedControlPoint();
        planarFigure->RemoveControlPoint( selectedControlPoint );

        // Re-evaluate features
        planarFigure->EvaluateFeatures();
        planarFigure->Modified();

        m_DataNode->SetBoolProperty( "planarfigure.drawcontrolpoints", true );
        planarFigure->InvokeEvent( EndInteractionPlanarFigureEvent() );
        renderer->GetRenderingManager()->RequestUpdateAll();
        this->HandleEvent( new mitk::StateEvent( EIDYES, NULL ) );
      }
      else
      {
        this->HandleEvent( new mitk::StateEvent( EIDNO, NULL ) );
      }
    }

    //case AcMOVEPOINT:
    //case AcMOVESELECTED:
    //  {
    //    // Update the display
    //    renderer->GetRenderingManager()->RequestUpdateAll();

    //    ok = true;
    //    break;
    //  }

    //case AcFINISHMOVE:
    //  {
    //    ok = true;
    //    break;
    //  }

  default:
    return Superclass::ExecuteAction( action, stateEvent );
  }




  return ok;
}

bool mitk::PlanarFigureInteractor::TransformPositionEventToPoint2D(
  const StateEvent *stateEvent, Point2D &point2D,
  const Geometry2D *planarFigureGeometry )
{
  // Extract world position, and from this position on geometry, if
  // available
  const mitk::PositionEvent *positionEvent =
    dynamic_cast< const mitk::PositionEvent * > ( stateEvent->GetEvent() );
  if ( positionEvent == NULL )
  {
    return false;
  }

  mitk::Point3D worldPoint3D = positionEvent->GetWorldPosition();

  // TODO: proper handling of distance tolerance
  if ( planarFigureGeometry->Distance( worldPoint3D ) > 0.1 )
  {
    return false;
  }

  // Project point onto plane of this PlanarFigure
  planarFigureGeometry->Map( worldPoint3D, point2D );
  return true;
}


bool mitk::PlanarFigureInteractor::TransformObjectToDisplay(
  const mitk::Point2D &point2D,
  mitk::Point2D &displayPoint,
  const mitk::Geometry2D *objectGeometry,
  const mitk::Geometry2D *rendererGeometry,
  const mitk::DisplayGeometry *displayGeometry ) const
{
  mitk::Point3D point3D;

  // Map circle point from local 2D geometry into 3D world space
  objectGeometry->Map( point2D, point3D );

  // TODO: proper handling of distance tolerance
  if ( displayGeometry->Distance( point3D ) < 0.1 )
  {
    // Project 3D world point onto display geometry
    rendererGeometry->Map( point3D, displayPoint );
    displayGeometry->WorldToDisplay( displayPoint, displayPoint );
    return true;
  }

  return false;
}


bool mitk::PlanarFigureInteractor::IsPointNearLine(
  const mitk::Point2D& point,
  const mitk::Point2D& startPoint,
  const mitk::Point2D& endPoint,
  mitk::Point2D& projectedPoint
  ) const
{
  mitk::Vector2D n1 = endPoint - startPoint;
  n1.Normalize();

  // Determine dot products between line vector and startpoint-point / endpoint-point vectors
  double l1 = n1 * (point - startPoint);
  double l2 = -n1 * (point - endPoint);

  // Determine projection of specified point onto line defined by start / end point
  mitk::Point2D crossPoint = startPoint + n1 * l1;
  projectedPoint = crossPoint;

  // Point is inside encompassing rectangle IF
  // - its distance to its projected point is small enough
  // - it is not further outside of the line than the defined tolerance
  if (((crossPoint.SquaredEuclideanDistanceTo(point) < 20.0) && (l1 > 0.0) && (l2 > 0.0))
      || endPoint.SquaredEuclideanDistanceTo(point) < 20.0
      || startPoint.SquaredEuclideanDistanceTo(point) < 20.0)
  {
    return true;
  }

  return false;
}


int mitk::PlanarFigureInteractor::IsPositionOverFigure(
  const StateEvent *stateEvent, PlanarFigure *planarFigure,
  const Geometry2D *planarFigureGeometry,
  const Geometry2D *rendererGeometry,
  const DisplayGeometry *displayGeometry,
  Point2D& pointProjectedOntoLine ) const
{
  // Extract display position
  const mitk::PositionEvent *positionEvent =
    dynamic_cast< const mitk::PositionEvent * > ( stateEvent->GetEvent() );
  if ( positionEvent == NULL )
  {
    return -1;
  }

  mitk::Point2D displayPosition = positionEvent->GetDisplayPosition();


  // Iterate over all polylines of planar figure, and check if
  // any one is close to the current display position
  typedef mitk::PlanarFigure::PolyLineType VertexContainerType;

  mitk::Point2D worldPoint2D, displayControlPoint;
  mitk::Point3D worldPoint3D;

  for ( unsigned short loop = 0; loop < planarFigure->GetPolyLinesSize(); ++loop )
  {
    const VertexContainerType polyLine = planarFigure->GetPolyLine( loop );

    Point2D polyLinePoint;
    Point2D firstPolyLinePoint;
    Point2D previousPolyLinePoint;

    bool firstPoint = true;
    for ( VertexContainerType::const_iterator it = polyLine.begin(); it != polyLine.end(); ++it )
    {
      // Get plane coordinates of this point of polyline (if possible)
      if ( !this->TransformObjectToDisplay( it->Point, polyLinePoint,
        planarFigureGeometry, rendererGeometry, displayGeometry ) )
      {
        break; // Poly line invalid (not on current 2D plane) --> skip it
      }

      if ( firstPoint )
      {
        firstPolyLinePoint = polyLinePoint;
        firstPoint = false;
      }
      else if ( this->IsPointNearLine( displayPosition, previousPolyLinePoint, polyLinePoint, pointProjectedOntoLine ) )
      {
        // Point is close enough to line segment --> Return index of the segment
        return it->Index;
      }
      previousPolyLinePoint = polyLinePoint;
    }

    // For closed figures, also check last line segment
    if ( planarFigure->IsClosed()
      && this->IsPointNearLine( displayPosition, polyLinePoint, firstPolyLinePoint, pointProjectedOntoLine ) )
    {
      return 0; // Return index of first control point
    }
  }

  return -1;
}


int mitk::PlanarFigureInteractor::IsPositionInsideMarker(
  const StateEvent *stateEvent, const PlanarFigure *planarFigure,
  const Geometry2D *planarFigureGeometry,
  const Geometry2D *rendererGeometry,
  const DisplayGeometry *displayGeometry ) const
{
  // Extract display position
  const mitk::PositionEvent *positionEvent =
    dynamic_cast< const mitk::PositionEvent * > ( stateEvent->GetEvent() );
  if ( positionEvent == NULL )
  {
    return -1;
  }

  mitk::Point2D displayPosition = positionEvent->GetDisplayPosition();


  // Iterate over all control points of planar figure, and check if
  // any one is close to the current display position
  mitk::Point2D worldPoint2D, displayControlPoint;
  mitk::Point3D worldPoint3D;

  int numberOfControlPoints = planarFigure->GetNumberOfControlPoints();
  for ( int i=0; i<numberOfControlPoints; i++ )
  {
    Point2D displayControlPoint;
    if ( this->TransformObjectToDisplay( planarFigure->GetControlPoint(i), displayControlPoint,
      planarFigureGeometry, rendererGeometry, displayGeometry ) )
    {
      // TODO: variable size of markers
      if ( displayPosition.SquaredEuclideanDistanceTo( displayControlPoint ) < 20.0 )
      {
        return i;
      }
    }
  }


  //for ( it = controlPoints.begin(); it != controlPoints.end(); ++it )
  //{
  //  Point2D displayControlPoint;
  //  if ( this->TransformObjectToDisplay( it->Point, displayControlPoint,
  //    planarFigureGeometry, rendererGeometry, displayGeometry ) )
  //  {
  //    // TODO: variable size of markers
  //    if ( (abs(displayPosition[0] - displayControlPoint[0]) < 4 )
  //      && (abs(displayPosition[1] - displayControlPoint[1]) < 4 ) )
  //    {
  //      return index;
  //    }
  //  }
  //}

  return -1;
}


void mitk::PlanarFigureInteractor::LogPrintPlanarFigureQuantities(
  const PlanarFigure *planarFigure )
{
  MITK_INFO << "PlanarFigure: " << planarFigure->GetNameOfClass();
  for ( unsigned int i = 0; i < planarFigure->GetNumberOfFeatures(); ++i )
  {
    MITK_INFO << "* " << planarFigure->GetFeatureName( i ) << ": "
      << planarFigure->GetQuantity( i ) << " " << planarFigure->GetFeatureUnit( i );
  }
}

bool
mitk::PlanarFigureInteractor::IsMousePositionAcceptableAsNewControlPoint(
    mitk::StateEvent const * stateEvent,
    const PlanarFigure* planarFigure )
{
  assert(stateEvent && planarFigure);

  BaseRenderer* renderer = stateEvent->GetEvent()->GetSender();

  assert(renderer);

  // Get the timestep to support 3D+t
  int timeStep( renderer->GetTimeStep( planarFigure ) );

  // Get current display position of the mouse
  //Point2D currentDisplayPosition = positionEvent->GetDisplayPosition();

  // Check if a previous point has been set
  bool tooClose = false;

  const Geometry2D *renderingPlane = renderer->GetCurrentWorldGeometry2D();

  mitk::Geometry2D *planarFigureGeometry =
    dynamic_cast< mitk::Geometry2D * >( planarFigure->GetGeometry( timeStep ) );

  Point2D point2D, correctedPoint;
  // Get the point2D from the positionEvent
  if ( !this->TransformPositionEventToPoint2D( stateEvent, point2D,
    planarFigureGeometry ) )
  {
    return false;
  }

  // apply the controlPoint constraints of the planarFigure to get the
  // coordinates that would actually be used.
  correctedPoint = const_cast<PlanarFigure*>( planarFigure )->ApplyControlPointConstraints( 0, point2D );

  // map the 2D coordinates of the new point to world-coordinates
  // and transform those to display-coordinates
  mitk::Point3D newPoint3D;
  planarFigureGeometry->Map( correctedPoint, newPoint3D );
  mitk::Point2D newDisplayPosition;
  renderingPlane->Map( newPoint3D, newDisplayPosition );
  renderer->GetDisplayGeometry()->WorldToDisplay( newDisplayPosition, newDisplayPosition );


  for( int i=0; i < (int)planarFigure->GetNumberOfControlPoints(); i++ )
  {
    if ( i != planarFigure->GetSelectedControlPoint() )
    {
      // Try to convert previous point to current display coordinates
      mitk::Point3D previousPoint3D;
      // map the 2D coordinates of the control-point to world-coordinates
      planarFigureGeometry->Map( planarFigure->GetControlPoint( i ), previousPoint3D );

      if ( renderer->GetDisplayGeometry()->Distance( previousPoint3D ) < 0.1 ) // ugly, but assert makes this work
      {
        mitk::Point2D previousDisplayPosition;
        // transform the world-coordinates into display-coordinates
        renderingPlane->Map( previousPoint3D, previousDisplayPosition );
        renderer->GetDisplayGeometry()->WorldToDisplay( previousDisplayPosition, previousDisplayPosition );

        //Calculate the distance. We use display-coordinates here to make
        // the check independent of the zoom-level of the rendering scene.
        double a = newDisplayPosition[0] - previousDisplayPosition[0];
        double b = newDisplayPosition[1] - previousDisplayPosition[1];

        // If point is to close, do not set a new point
        tooClose = (a * a + b * b < m_MinimumPointDistance );
      }
      if ( tooClose )
        return false; // abort loop early
    }
  }

  return !tooClose; // default
}
