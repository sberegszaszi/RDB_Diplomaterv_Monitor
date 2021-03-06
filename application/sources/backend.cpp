//==============================================================================//
//                                                                              //
//    RDB Diplomaterv Monitor                                                   //
//    A monitor program for the RDB Diplomaterv project                         //
//    Copyright (C) 2018  András Gergő Kocsis                                   //
//                                                                              //
//    This program is free software: you can redistribute it and/or modify      //
//    it under the terms of the GNU General Public License as published by      //
//    the Free Software Foundation, either version 3 of the License, or         //
//    (at your option) any later version.                                       //
//                                                                              //
//    This program is distributed in the hope that it will be useful,           //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of            //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             //
//    GNU General Public License for more details.                              //
//                                                                              //
//    You should have received a copy of the GNU General Public License         //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.    //
//                                                                              //
//==============================================================================//



#include "backend.hpp"
#include "main_window.hpp"
#include "network_handler.hpp"
#include "serial_port.hpp"
#include "measurement_data_protocol.hpp"



Backend::Backend() : QObject(),
                     serial_port(),
                     measurement_data_protocol(),
                     serial_network_handler(&serial_port,
                                            &measurement_data_protocol,
                                            std::bind(&Backend::StoreNetworkDiagrams, this, std::placeholders::_1, std::placeholders::_2),
                                            std::bind(&Backend::ReportStatus, this, std::placeholders::_1)),
                     gui_signal_interface(nullptr)
{
// #warning "This function needs to be changed when implementing the generic protocol handling"
}

void Backend::RegisterGuiSignalInterface(GuiSignalInterface* new_gui_signal_interface)
{
    if(new_gui_signal_interface)
    {
        gui_signal_interface = new_gui_signal_interface;

        QObject::connect(dynamic_cast<QObject*>(gui_signal_interface),  SIGNAL(OpenNetworkConnection(const std::string&)),
                         this,                                          SLOT(OpenNetwokConnection(const std::string&)));
        QObject::connect(dynamic_cast<QObject*>(gui_signal_interface),  SIGNAL(CloseNetworkConnection(const std::string&)),
                         this,                                          SLOT(CloseNetworkConnection(const std::string&)));
        QObject::connect(dynamic_cast<QObject*>(gui_signal_interface),  SIGNAL(RequestForDiagram(const QModelIndex&)),
                         this,                                          SLOT(RequestForDiagram(const QModelIndex&)));
        QObject::connect(dynamic_cast<QObject*>(gui_signal_interface),  SIGNAL(ImportFile(const std::string&)),
                         this,                                          SLOT(ImportFile(const std::string&)));
        QObject::connect(dynamic_cast<QObject*>(gui_signal_interface),  SIGNAL(ExportFileShowCheckBoxes(void)),
                         this,                                          SLOT(ExportFileShowCheckBoxes(void)));
        QObject::connect(dynamic_cast<QObject*>(gui_signal_interface),  SIGNAL(ExportFileHideCheckBoxes(void)),
                         this,                                          SLOT(ExportFileHideCheckBoxes(void)));
        QObject::connect(dynamic_cast<QObject*>(gui_signal_interface),  SIGNAL(ExportFileStoreCheckedDiagrams(const std::string&)),
                         this,                                          SLOT(ExportFileStoreCheckedDiagrams(const std::string&)));

    }
    else
    {
        std::string errorMessage = "There was no gui_signal_interface set in Backend::RegisterGuiSignalInterface!";
        throw errorMessage;
    }
}

void Backend::ReportStatus(const std::string& message)
{
    // The format string needs to be defined as a string literal so that the compiler can check it's content
    #define STATUS_REPORT_FORMAT_STRING ("%02d:%02d:%02d - %s")
    // This is the pre-calculated length of the status report, this can be used for checks
    std::size_t status_report_length = sizeof("HH:MM:SS - ") + message.size();

    // Getting the current time
    // Under Windows, the warning might arise that the localtime() is deprecated and the localtime_s() should be used instead.
    // This warning will be ignored because GCC does not support it and for keeping the code simple.
    time_t raw_time;
    struct tm* local_time_values;
    raw_time = time(nullptr);
    local_time_values = localtime(&raw_time);

    // Assembling the status message from the time and the input text
    auto report_message = std::make_unique<char[]>(status_report_length);
    auto snprintf_result = snprintf(report_message.get(), status_report_length, STATUS_REPORT_FORMAT_STRING,
                                    local_time_values->tm_hour,
                                    local_time_values->tm_min,
                                    local_time_values->tm_sec,
                                    message.c_str());

    // If everything was ok with the snprintf call
    // We can not check it with the report_message because of the coding,
    // the sizes will not necessarily match with the return value of the snprintf()
    if(0 < snprintf_result)
    {
        emit NewStatusMessage(std::string(report_message.get()));
    }
    else
    {
        throw("ERROR! The snprintf() has reported a format error in Backend::ReportStatus()!");
    }
}

void Backend::StoreNetworkDiagrams(const std::string& connection_name, std::vector<DiagramSpecialized>& new_diagrams)
{
    StoreDiagrams(new_diagrams,
        [&](const DiagramSpecialized& diagram_to_add) -> QModelIndex
        {
            return diagram_container.AddDiagramFromNetwork(connection_name, diagram_to_add);
        });
}

void Backend::StoreFileDiagrams(const std::string& file_name, const std::string& file_path, std::vector<DiagramSpecialized>& new_diagrams)
{
    StoreDiagrams(new_diagrams,
        [&](const DiagramSpecialized& diagram_to_add) -> QModelIndex
        {
            return diagram_container.AddDiagramFromFile(file_name, file_path, diagram_to_add);
        });
}

std::vector<std::string> Backend::GetSupportedFileExtensions(void)
{
// #warning "This function needs to be changed when implementing the generic protocol handling"
    std::vector<std::string> result;

    result.push_back(measurement_data_protocol.GetSupportedFileType());

    return result;
}

void Backend::OpenNetwokConnection(const std::string& port_name)
{
    bool result = false;

    if(serial_network_handler.Run(port_name))
    {
        result = true;
        ReportStatus("The connection \"" + port_name + "\" was successfully opened!");
    }
    else
    {
        ReportStatus("The connection \"" + port_name + "\" could not be opened...maybe wrong name?");
    }
    emit NetworkOperationFinished(port_name, result);
}

void Backend::CloseNetworkConnection(const std::string& port_name)
{
    serial_network_handler.Stop();

    ReportStatus("The connection \"" + port_name + "\" was successfully closed!");
    NetworkOperationFinished(port_name, true);
}

void Backend::RequestForDiagram(const QModelIndex& model_index)
{
    DiagramSpecialized* first_diagram = diagram_container.GetDiagram(model_index);
    if(first_diagram)
    {
        emit ShowThisDiagram(*first_diagram);
    }
}

void Backend::ImportFile(const std::string& path_to_file)
{
// #warning "This function needs to be changed when implementing the generic protocol handling"
    QFileInfo file_info(QString::fromStdString(path_to_file));
    if(file_info.exists())
    {
        std::string file_name = file_info.fileName().toStdString();
        if(!diagram_container.IsThisFileAlreadyStored(file_name, path_to_file))
        {
            if(measurement_data_protocol.CanThisFileBeProcessed(path_to_file))
            {
                std::ifstream file_stream(path_to_file);
                auto diagrams_from_file = measurement_data_protocol.ProcessData(file_stream);
                StoreFileDiagrams(file_name, path_to_file, diagrams_from_file);

                // Updating the configuration with the folder of the file that was imported
                configuration.ImportFolder(file_info.absoluteDir().absolutePath().toStdString());

                ReportStatus("The file \"" + path_to_file + "\" was successfully opened!");
            }
            else
            {
                ReportStatus("ERROR! The MeasurementDataProtocol cannot process the file: \"" + path_to_file + "\" because it has a wrong extension!");
            }
        }
        else
        {
            ReportStatus("The file \"" + path_to_file + "\" was already imported and will not be imported again!");
        }
    }
    else
    {
        ReportStatus("ERROR! The path \"" + path_to_file + "\" does not exist!");
    }
}

void Backend::ExportFileShowCheckBoxes(void)
{
    diagram_container.ShowCheckBoxes();
}

void Backend::ExportFileHideCheckBoxes(void)
{
    diagram_container.HideCheckBoxes();
}

void Backend::ExportFileStoreCheckedDiagrams(const std::string& path_to_file)
{
// #warning "This function needs to be changed when implementing the generic protocol handling"
    if(measurement_data_protocol.CanThisFileBeProcessed(path_to_file))
    {
        auto checked_diagrams = diagram_container.GetCheckedDiagrams();
        if(checked_diagrams.size())
        {
            auto exported_data = measurement_data_protocol.ExportData(checked_diagrams);

            std::ofstream output_file_stream(path_to_file, (std::ofstream::out | std::ofstream::trunc));
            output_file_stream << exported_data.rdbuf();

            // Updating the configuration with the folder of the file that was exported
            configuration.ExportFolder(QFileInfo(QString::fromStdString(path_to_file)).absoluteDir().absolutePath().toStdString());

            ReportStatus("The selected diagrams were successfully written to \"" + path_to_file + "\"!");
        }
        else
        {
            ReportStatus("No diagram was selected! Nothing was exported!");
        }
    }
    else
    {
        ReportStatus("ERROR! The MeasurementDataProtocol cannot save diagrams into the file: \"" + path_to_file + "\" because it has a wrong extension!");
    }
}

void Backend::StoreDiagrams(std::vector<DiagramSpecialized>& new_diagrams, const std::function<QModelIndex(const DiagramSpecialized&)> storage_logic)
{
    auto container_is_empty = (0 == diagram_container.GetNumberOfDiagrams());

    // Adding the diagrams to the diagram_container
    for(const auto& i : new_diagrams)
    {
        // Calling the logic that does the storage for a single diagram, this is provided by the caller
        auto recently_added_diagram = storage_logic(i);

        // Displaying the diagram if this was the first
        if(container_is_empty)
        {
            container_is_empty = false;

            DiagramSpecialized* first_diagram = diagram_container.GetDiagram(recently_added_diagram);
            if(first_diagram)
            {
                emit ShowThisDiagram(*first_diagram);
            }
        }
    }

    ReportStatus(std::to_string(new_diagrams.size()) + " new diagram was added to the list.");
}
