﻿
using System;
using System.Collections.Generic;
using System.Linq;
using Badger.Data;
using Caliburn.Micro;

using Herd.Files;
using Herd.Network;

namespace Badger.ViewModels
{
    public class MonitoredExperimentalUnitViewModel : PropertyChangedBase
    {
        static int NextTaskId { get; set; } = 0;

        private ExperimentalUnit m_model;

        public string PipeName { get; set; }

        public string Name => m_model.Name;

        public string ExperimentFileName => m_model.ExperimentFileName;

        public string TaskName { get; set; }

        public List<AppVersion> AppVersions => m_model.AppVersions;

        public AppVersion SelectedVersion => m_model.SelectedVersion;

        public DateTime LastHeartbeat;

        public string TimeSinceLastHeartbeat
        {
            get
            {
                TimeSpan timeElapsed = new TimeSpan();
                timeElapsed= (DateTime.Now - LastHeartbeat);
                if (timeElapsed.Hours>0)
                    return timeElapsed.Hours.ToString() + "h";
                if (timeElapsed.Minutes > 0)
                    return timeElapsed.Minutes.ToString() + "m";
                if (timeElapsed.Seconds > 0)
                    return timeElapsed.Seconds.ToString() + "s";

                return "<1s";
            }
        }

        public List<string> Forks
        {
            get
            {
                return m_model.ForkValues.Select(k => k.Key + ": " + k.Value).ToList();
            }
        }
        public string ForksAsString
        {
            get { return Herd.Utils.ListAsString(Forks); }
        }

        //STATE
        public MonitoredExperimentStateViewModel StateButton { get; set; } = new MonitoredExperimentStateViewModel();

        private Monitoring.State m_state = Monitoring.State.UNITIALIZED;
        public Monitoring.State State
        {
            get { return m_state; }
            set
            {
                //if a task within a job fails, we don't want to overwrite it's state when the job finishes
                //we don't update state in case new state is not RUNNING or SENDING
                if (m_state != Monitoring.State.ERROR)
                    m_state = value;
                NotifyOfPropertyChange(() => State);
                NotifyOfPropertyChange(() => IsRunning);
                NotifyOfPropertyChange(() => StateString);
                NotifyOfPropertyChange(() => StateColor);
                StateButton.Icon= StateString;
            }
        }

        public bool IsRunning { get { return m_state == Monitoring.State.RUNNING; } }

        public string StateString
        {
            get
            {
                switch (m_state)
                {
                    case Monitoring.State.RUNNING: return "Running";
                    case Monitoring.State.FINISHED: return "Finished";
                    case Monitoring.State.ERROR: return "Error";
                    case Monitoring.State.SENDING: return "Sending";
                    case Monitoring.State.RECEIVING: return "Receiving";
                    case Monitoring.State.WAITING_RESULT: return "Awaiting";
                    default:
                        return null;
                }
            }
        }

        public string StateColor
        {
            get
            {
                switch (m_state)
                {
                    case Monitoring.State.RUNNING:
                    case Monitoring.State.SENDING:
                    case Monitoring.State.RECEIVING:
                    case Monitoring.State.WAITING_RESULT:
                        return "Black";
                    case Monitoring.State.FINISHED: return "DarkGreen";
                    case Monitoring.State.ERROR: return "Red";
                }
                return "Black";
            }
        }

        //progress is expressed as a percentage

        private double m_progress = 0.0;
        public double Progress
        {
            get
            {
                return m_progress;
            }
            set
            {
                m_progress = value;
                NotifyOfPropertyChange(() => Progress);
            }
        }

        private string m_statusInfo;
        public string StatusInfo
        {
            get { return m_statusInfo; }
            set
            {
                m_statusInfo = value;
                NotifyOfPropertyChange(() => StatusInfo);
            }
        }

        public void ShowLog()
        {
            CaliburnUtility.ShowWarningDialog(StatusInfo, "Experiment log");
        }

        public void AddStatusInfoLine(string line)
        {
            StatusInfo += line + "\n";
        }


        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="expUnit">The model: an instance of ExperimentalUnit</param>
        /// <param name="plot">The plot used to monitor data</param
        public MonitoredExperimentalUnitViewModel(ExperimentalUnit expUnit, PlotViewModel plot)
        {
            StateButton.Icon = "Sending";

            TaskName = "#" + NextTaskId;
            NextTaskId++;

            m_model = expUnit;
            PipeName = m_model.Name;
            m_plotEvaluationMonitor = plot;

            LastHeartbeat = DateTime.Now;
        }



        //evaluation plot stuff
        private int m_seriesId = -1;
        public PlotViewModel m_plotEvaluationMonitor;

        public void AddEvaluationValue(double xNorm, double y)
        {
            if (m_seriesId == -1) //series not yet added
                m_seriesId = m_plotEvaluationMonitor.AddLineSeries(Name);
            m_plotEvaluationMonitor.AddLineSeriesValue(m_seriesId, xNorm, y);
        }
    }
}
