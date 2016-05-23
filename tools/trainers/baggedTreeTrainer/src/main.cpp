////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Project:  Embedded Machine Learning Library (EMLL)
//  File:     main.cpp (baggedTreeLearner)
//  Authors:  Ofer Dekel
//
////////////////////////////////////////////////////////////////////////////////////////////////////


// utilities
#include "Files.h"
#include "OutputStreamImpostor.h"
#include "CommandLineParser.h" 
#include "RandomEngines.h"
#include "BinaryClassificationEvaluator.h"

// layers
#include "Map.h"
#include "Coordinate.h"
#include "CoordinateListTools.h"

// dataset
#include "SupervisedExample.h"

// common
#include "SortingTreeTrainerArguments.h"
#include "BaggingIncrementalTrainerArguments.h"
#include "TrainerArguments.h"
#include "MapLoadArguments.h" 
#include "MapSaveArguments.h" 
#include "DataLoadArguments.h" 
#include "DataLoaders.h"
#include "LoadModel.h"
#include "MakeTrainer.h"
#include "MakeEvaluator.h"

// trainers
#include "SortingTreeTrainer.h"

// lossFunctions
#include "SquaredLoss.h"
#include "LogLoss.h"

// stl
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[])
{
    try
    {
        // create a command line parser
        utilities::CommandLineParser commandLineParser(argc, argv);

        // add arguments to the command line parser
        common::ParsedTrainerArguments trainerArguments;
        common::ParsedMapLoadArguments mapLoadArguments;
        common::ParsedDataLoadArguments dataLoadArguments;
        common::ParsedMapSaveArguments mapSaveArguments;
        common::ParsedSortingTreeTrainerArguments sortingTreeTrainerArguments;
        common::ParsedBaggingIncrementalTrainerArguments baggingIncrementalTrainerArguments;

        commandLineParser.AddOptionSet(trainerArguments);
        commandLineParser.AddOptionSet(mapLoadArguments);
        commandLineParser.AddOptionSet(dataLoadArguments);
        commandLineParser.AddOptionSet(mapSaveArguments);
        commandLineParser.AddOptionSet(sortingTreeTrainerArguments);
        commandLineParser.AddOptionSet(baggingIncrementalTrainerArguments);
        
        // parse command line
        commandLineParser.Parse();

        if(trainerArguments.verbose)
        {
            std::cout << "Bagging Tree Trainer" << std::endl;
            std::cout << commandLineParser.GetCurrentValuesString() << std::endl;
        }

        // if output file specified, replace stdout with it 
        auto outStream = utilities::GetOutputStreamImpostor(mapSaveArguments.outputModelFile);

        // load a model
        auto model = common::LoadModel(mapLoadArguments.modelLoadArguments);

        // get output coordinate list and create the map
        auto outputCoordinateList = layers::BuildCoordinateList(model, dataLoadArguments.parsedDataDimension, mapLoadArguments.coordinateListString);
        layers::Map map(model, outputCoordinateList);

        // load dataset
        if(trainerArguments.verbose) std::cout << "Loading data ..." << std::endl;
        auto rowDataset = common::GetRowDataset(dataLoadArguments, std::move(map));

        // create trainer
        auto baseTrainer = common::MakeSortingTreeTrainer(trainerArguments.lossArguments, sortingTreeTrainerArguments);
        auto trainer = trainers::MakeBaggingIncrementalTrainer(std::move(baseTrainer), baggingIncrementalTrainerArguments);
        
        // train
        if(trainerArguments.verbose) std::cout << "Training ..." << std::endl;
        auto trainSetIterator = rowDataset.GetIterator();
        trainer->Update(trainSetIterator);
        auto predictor = trainer->GetPredictor();

        // print loss and errors
        if(trainerArguments.verbose)
        {
            std::cout << "Finished training.\n";

            auto evaluator = common::MakeBinaryClassificationEvaluator<predictors::EnsemblePredictor<predictors::DecisionTreePredictor>>(trainerArguments.lossArguments);
            auto evaluationIterator = rowDataset.GetIterator();
            evaluator->Evaluate(evaluationIterator, *predictor);

            std::cout << "Training error\n";
            evaluator->Print(std::cout);
            std::cout << std::endl;
        }

        // add predictor to the model
        predictor->AddToModel(model, outputCoordinateList);

        // save the model
        model.Save(outStream);

    }
    catch (const utilities::CommandLineParserPrintHelpException& exception)
    {
        std::cout << exception.GetHelpText() << std::endl;
        return 0;
    }
    catch (const utilities::CommandLineParserErrorException& exception)
    {
        std::cerr << "Command line parse error:" << std::endl;
        for (const auto& error : exception.GetParseErrors())
        {
            std::cerr << error.GetMessage() << std::endl;
        }
        return 1;
    }
    catch (std::runtime_error exception)
    {
        std::cerr << "runtime error: " << exception.what() << std::endl;
        return 1;
    }

    return 0;
}
