//==========================================================================
//  AIDA Detector description implementation 
//--------------------------------------------------------------------------
// Copyright (C) Organisation europeenne pour la Recherche nucleaire (CERN)
// All rights reserved.
//
// For the licensing terms see $DD4hepINSTALL/LICENSE.
// For the list of contributors see $DD4hepINSTALL/doc/CREDITS.
//
// Author     : M.Frank
//
//==========================================================================

/// Framework include files
#include <DD4hep/InstanceCount.h>
#include <DDDigi/DigiContext.h>
#include <DDDigi/DigiPlugins.h>
#include <DDDigi/DigiKernel.h>
#include "DigiEdm4hepOutput.h"
#include "DigiIO.h"

/// edm4hep include files
#include <podio/EventStore.h>
#include <podio/ROOTWriter.h>
#include <edm4hep/SimTrackerHit.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/TrackerHitCollection.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/CalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>


/// Namespace for the AIDA detector description toolkit
namespace dd4hep {

  /// Namespace for the Digitization part of the AIDA detector description toolkit
  namespace digi {

    /// Helper class to create output in edm4hep format
    /** Helper class to create output in edm4hep format
     *
     *  \author  M.Frank
     *  \version 1.0
     *  \ingroup DD4HEP_DIGITIZATION
     */
    class DigiEdm4hepOutput::internals_t {
    public:
      DigiEdm4hepOutput* m_parent                    { nullptr };
      /// Reference to podio store
      std::unique_ptr<podio::EventStore>  m_store     { };
      /// Reference to podio writer
      std::unique_ptr<podio::ROOTWriter>  m_file      { };
      /// edm4hep event header collection
      edm4hep::EventHeaderCollection*     m_header    { nullptr };
      /// MC particle collection
      edm4hep::MCParticleCollection*      m_particles { nullptr };
      /// Collection of all edm4hep object collections
      std::map<std::string, podio::CollectionBase*> m_collections;

      /// Total numbe rof events to be processed
      long num_events  { -1 };
      /// Running event counter
      long event_count {  0 };

    private:
      /// Helper to register single collection
      template <typename T> T* register_collection(const std::string& name, T* collection);

    public:
      /// Default constructor
      internals_t(DigiEdm4hepOutput* parent);
      /// Default destructor
      ~internals_t();

      /// Commit data at end of filling procedure
      void commit();
      /// Open new output stream
      void open();
      /// Commit data to disk and close output stream
      void close();

      /// Create all collections according to the parent setup (locked)
      void create_collections();
      /// Access named collection: throws exception ifd the collection is not present (unlocked!)
      template <typename T> podio::CollectionBase* get_collection(const T&);
    };

    /// Default constructor
    DigiEdm4hepOutput::internals_t::internals_t(DigiEdm4hepOutput* parent) : m_parent(parent)
    {
      m_store  = std::make_unique<podio::EventStore>();
    }

    /// Default destructor
    DigiEdm4hepOutput::internals_t::~internals_t()    {
      if ( m_file ) close();
      m_store.reset();
    }

    template <typename T> T* DigiEdm4hepOutput::internals_t::register_collection(const std::string& nam, T* coll)   {
      m_collections.emplace(nam, coll);
      m_store->registerCollection(nam, coll);
      m_parent->debug("+++ created collection %s <%s>", nam.c_str(), coll->getTypeName().c_str());
      return coll;
    }

    /// Create all collections according to the parent setup
    void DigiEdm4hepOutput::internals_t::create_collections()    {
      if ( nullptr == m_header )   {
        m_header = register_collection("EventHeader", new edm4hep::EventHeaderCollection());
        for( auto& cont : m_parent->m_containers )   {
          const std::string& nam = cont.first;
          const std::string& typ = cont.second;
          if ( typ == "MCParticles" )   {
            m_particles = register_collection(nam, new edm4hep::MCParticleCollection());
          }
          else if ( typ == "TrackerHits" )   {
            register_collection(nam, new edm4hep::TrackerHitCollection());
          }
          else if ( typ == "CalorimeterHits" )   {
            register_collection(nam, new edm4hep::CalorimeterHitCollection());
          }
        }
        m_parent->info("+++ Will save %ld events to %s", num_events, m_parent->m_output.c_str());
      }
    }

    /// Access named collection: throws exception ifd the collection is not present
    template <typename T> 
    podio::CollectionBase* DigiEdm4hepOutput::internals_t::get_collection(const T& cont)  {
      auto iter = m_collections.find(cont.name);
      if ( iter == m_collections.end() )    {
        m_parent->except("Error");
      }
      return iter->second;
    }

    /// Commit data at end of filling procedure
    void DigiEdm4hepOutput::internals_t::commit()   {
      if ( m_file )   {
        m_file->writeEvent();
        m_store->clearCollections();
        return;
      }
      m_parent->except("+++ Failed to write output file. [Stream is not open]");
    }

    /// Open new output stream
    void DigiEdm4hepOutput::internals_t::open()    {
      if ( m_file )   {
        close();
      }
      m_file.reset();
      std::string fname = m_parent->next_stream_name();
      m_file = std::make_unique<podio::ROOTWriter>(fname, m_store.get());
      m_parent->info("+++ Opened EDM4HEP output file %s", fname.c_str());
      for( const auto& c : m_collections )   {
	m_file->registerForWrite(c.first);
      }
    }

    /// Commit data to disk and close output stream
    void DigiEdm4hepOutput::internals_t::close()   {
      m_parent->info("+++ Closing EDM4HEP output file.");
      if ( m_file )   {
        m_file->finish();
      }
      m_file.reset();
    }

    /// Standard constructor
    DigiEdm4hepOutput::DigiEdm4hepOutput(const DigiKernel& krnl, const std::string& nam)
      : DigiOutputAction(krnl, nam)
    {
      internals = std::make_shared<internals_t>(this);
      InstanceCount::increment(this);
    }

    /// Default destructor
    DigiEdm4hepOutput::~DigiEdm4hepOutput()   {
      internals.reset();
      InstanceCount::decrement(this);
    }

    /// Initialization callback
    void DigiEdm4hepOutput::initialize()   {
      this->DigiOutputAction::initialize();
      for ( auto& c : m_registered_processors )   {
        auto* act = dynamic_cast<DigiEdm4hepOutputProcessor*>(c.second);
        if ( act )   { // This is not nice! Need to think about something better.
          act->internals = this->internals;
	  continue;
        }
	except("Error: Invalid processor type for EDM4HEP output: %s", c.second->c_name());
      }
      m_parallel = false;
      internals->create_collections();
    }

    /// Check for valid output stream
    bool DigiEdm4hepOutput::have_output()  const  {
      return internals->m_file.get() != nullptr;
    }

    /// Open new output stream
    void DigiEdm4hepOutput::open_output() const   {
      internals->open();
    }

    /// Close possible open stream
    void DigiEdm4hepOutput::close_output()  const  {
      internals->close();
    }

    /// Commit event data to output stream
    void DigiEdm4hepOutput::commit_output() const  {
      internals->commit();
    }

    /// Standard constructor
    DigiEdm4hepOutputProcessor::DigiEdm4hepOutputProcessor(const DigiKernel& krnl, const std::string& nam)
      : DigiContainerProcessor(krnl, nam)
    {
      declareProperty("point_resolution_RPhi", m_pointResoutionRPhi);
      declareProperty("point_resolution_Z",    m_pointResoutionZ);
      declareProperty("hit_type",              m_hit_type = 0);
    }

    void DigiEdm4hepOutputProcessor::convert_particles(DigiContext& ctxt,
						       const ParticleMapping& cont)  const
    {
      auto* coll = internals->m_particles;
      std::size_t start = coll->size();
      data_io<edm4hep_input>::_to_edm4hep(cont, coll);
      std::size_t end = internals->m_particles->size();
      info("%s+++ %-24s added %6ld/%6ld entries from mask: %04X to %s",
           ctxt.event->id(), cont.name.c_str(), end-start, end, cont.key.mask(),
           coll->getTypeName().c_str());
    }

    template <typename T> void
    DigiEdm4hepOutputProcessor::convert_depos(const T& cont,
					      const predicate_t& predicate,
					      edm4hep::TrackerHitCollection* collection)  const
    {
      std::array<float,6> covMat = {0., 0., m_pointResoutionRPhi*m_pointResoutionRPhi, 
				    0., 0., m_pointResoutionZ*m_pointResoutionZ
      };
      for ( const auto& depo : cont )   {
        if ( predicate(depo) )   {
          data_io<edm4hep_input>::_to_edm4hep(depo, covMat, *collection, m_hit_type /* edm4hep::SIMTRACKERHIT */);
        }
      }
    }

    template <typename T> void
    DigiEdm4hepOutputProcessor::convert_depos(const T& cont,
					      const predicate_t& predicate,
					      edm4hep::CalorimeterHitCollection* collection)  const
    {
      for ( const auto& depo : cont )   {
        if ( predicate(depo) )   {
          data_io<edm4hep_input>::_to_edm4hep(depo, *collection, m_hit_type /* edm4hep::SIMCALORIMETERHIT */);
        }
      }
    }

    template <typename T> void
    DigiEdm4hepOutputProcessor::convert_deposits(DigiContext&       ctxt,
						 const T&           cont,
						 const predicate_t& predicate)  const
    {
      podio::CollectionBase* coll = internals->get_collection(cont);
      std::size_t start = coll->size();
      if ( !cont.empty() )   {
        switch(cont.data_type)    {
        case SegmentEntry::TRACKER_HITS:
          convert_depos(cont, predicate, static_cast<edm4hep::TrackerHitCollection*>(coll));
          break;
        case SegmentEntry::CALORIMETER_HITS:
          convert_depos(cont, predicate, static_cast<edm4hep::CalorimeterHitCollection*>(coll));
          break;
        default:
          except("Error: Unknown energy deposit type: %d", int(cont.data_type));
          break;
        }
      }
      std::size_t end = coll->size();
      info("%s+++ %-24s added %6ld/%6ld entries from mask: %04X to %s",
           ctxt.event->id(), cont.name.c_str(), end-start, end, cont.key.mask(),
           coll->getTypeName().c_str());
    }

    void DigiEdm4hepOutputProcessor::convert_history(DigiContext&           ctxt,
						     const DepositsHistory& cont,
						     work_t&                work,
						     const predicate_t&     predicate)  const
    {
      info("%s+++ %-32s Segment: %d Predicate:%s Conversion to edm4hep not implemented!",
           ctxt.event->id(), cont.name.c_str(), int(work.input.segment->id),
           typeName(typeid(predicate)).c_str());
    }

    /// Main functional callback
    void DigiEdm4hepOutputProcessor::execute(DigiContext& ctxt, work_t& work, const predicate_t& predicate)  const  {
      if ( const auto* p = work.get_input<ParticleMapping>() )
        convert_particles(ctxt, *p);
      else if ( const auto* m = work.get_input<DepositMapping>() )
        convert_deposits(ctxt, *m, predicate);
      else if ( const auto* v = work.get_input<DepositVector>() )
        convert_deposits(ctxt, *v, predicate);
      else if ( const auto* h = work.get_input<DepositsHistory>() )
        convert_history(ctxt, *h, work, predicate);
      else
        except("Request to handle unknown data type: %s", work.input_type_name().c_str());
    }

  }    // End namespace digi
}      // End namespace dd4hep

/// Factory instantiation:
#include <DDDigi/DigiFactories.h>
DECLARE_DIGIACTION_NS(dd4hep::digi,DigiEdm4hepOutput)
DECLARE_DIGIACTION_NS(dd4hep::digi,DigiEdm4hepOutputProcessor)
