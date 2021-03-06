/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2012 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Application
    yPlus2T

Description
    Calculates and reports yPlus for all wall patches, for the specified times.

    Default behaviour assumes operating with a turbulent flow.

\*---------------------------------------------------------------------------*/
#include "fvCFD.H"

#include "rho2HTCModel.H"
#include "turbulentFluidThermoModel.H"

#include "nearWallDist.H"
#include "wallDist.H"
#include "wallFvPatch.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
using namespace std;

void calcIncompressibleYPlus
(
    const fvMesh& mesh,
    const Time& runTime,
    const volVectorField& U,
    volScalarField& yPlus2T,
    const bool& laminar
)
{}


void calcCompressibleYPlus
(
    const fvMesh& mesh,
    const Time& runTime,
    const volVectorField& U,
    volScalarField& yPlus2T,
    const bool& laminar
)
{
    IOobject rhoHeader
    (
        "rho",
        runTime.timeName(),
        mesh,
        IOobject::MUST_READ,
        IOobject::NO_WRITE
    );

    if (!rhoHeader.typeHeaderOk<volScalarField>(false))
    {
        Info<< "    no rho field" << endl;
        return;
    }

    Info<< "Reading field rho" << endl;
    volScalarField rho(rhoHeader, mesh);
    
    IOobject muHeader
    (
        "mu",
        runTime.timeName(),
        mesh,
        IOobject::MUST_READ,
        IOobject::NO_WRITE
    );
    
    if (!muHeader.typeHeaderOk<volScalarField>(false))
    {
        Info<< "    no mu field" << endl;
        return;
    }
    
    Info<< "Reading field mu" << endl;
    volScalarField mu(muHeader, mesh);
    
    #include "compressibleCreatePhi.H"

    autoPtr<hTC2Models::rho2HTCModel> reaction
    (
        hTC2Models::rho2HTCModel::New(mesh)
    );
    rho2ReactionThermo& thermo = reaction->thermo();
    
    volScalarField muEff = mu; //thermo.mu();

    if (!laminar)
    {
        autoPtr<compressible::turbulenceModel> turbulence
        (
          compressible::turbulenceModel::New
          (
              rho,
              U,
              phi,
              thermo
          )
        );
        
        IOobject mutHeader
        (
            "mut",
            runTime.timeName(),
            mesh,
            IOobject::MUST_READ,
            IOobject::NO_WRITE
        );
        
        if (!mutHeader.typeHeaderOk<volScalarField>(false))
        {
            Info<< "    no mut field" << endl;
            return;
        }

        Info<< "Reading field mut" << endl;
        volScalarField mut(mutHeader, mesh);

        muEff += mut; //turbulence->mut()();     
    }
    
    volSymmTensorField realTau 
    ( 
        -2.0/3.0 * muEff * fvc::div(U)*I //true for mono-atomic gases
        + muEff * twoSymm(fvc::grad(U))  
    );
    
   
    const volScalarField::Boundary wallPatches = rho.boundaryField();

    forAll(wallPatches, patchi)
    {
      if (isA<wallFvPatch>(wallPatches[patchi].patch()))
      {  
       
          /*dimensionedVector myIdentityVector 
          (   
              "myIdentityVector",
              dimless,             
              vector (1.0, 1.0, 1.0)
          );*/ //UNUSED
          
          //Any curved walls
          surfaceVectorField realTau_w 
          ( 
              -fvc::interpolate( realTau ) & mesh.Sf() / mesh.magSf()
          );   
          /*surfaceVectorField realTau_w 
          ( 
              fvc::interpolate( realTau ) & (mesh.Sf()^myIdentityVector) / mesh.magSf()
          );*/ //OLD FORMULATION VINCENT
          
          surfaceScalarField realuStar 
          ( 
              sqrt( mag(realTau_w) / fvc::interpolate(rho) ) 
          );       
                   
          volScalarField::Boundary y = nearWallDist(mesh).y();
          //Info<< "y = " << y[patchi] << nl << endl;
          
          surfaceScalarField yPl 
          ( 
              fvc::interpolate(rho) * realuStar
              / fvc::interpolate(muEff)
          ); 
          
          yPlus2T.boundaryFieldRef()[patchi] = y[patchi] * yPl.boundaryField()[patchi];
          const scalarField& Yp = yPlus2T.boundaryField()[patchi];

          Info<< "Patch " << patchi
              << " named " << wallPatches[patchi].patch().name()
              << " \e[1;33my+\e[0m : min: " << gMin(Yp) << "\e[1;33m max: " << gMax(Yp)
              << "\e[0m average: " << gAverage(Yp) << endl;
      }
      else
      {
          Info<< "\e[0;34mPatch " << patchi
          << " named " << wallPatches[patchi].patch().name()
          << " is not a wall\e[0m " << endl;
      } 
    }

}


int main(int argc, char *argv[])
{
    timeSelector::addOptions();

    #include "addRegionOption.H"

    argList::addBoolOption
    (
        "incompressible",
        "calculate incompressible y+"
    );
    
    argList::addBoolOption
    (
        "laminar",
        "calculate laminar y+"
    );

    #include "setRootCase.H"
    #include "createTime.H"
    instantList timeDirs = timeSelector::select0(runTime, args);
    #include "createNamedMesh.H"

    const bool incompressible = false; //args.optionFound("incompressible");
    const bool laminar = args.optionFound("laminar");

    forAll(timeDirs, timeI)
    {
        runTime.setTime(timeDirs[timeI], timeI);
        Info<< "Time = " << runTime.timeName() << endl;

        volScalarField yPlus
        (
            IOobject
            (
                "yPlus",
                runTime.timeName(),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE
            ),
            mesh,
            dimensionedScalar("yPlus", dimless, 0.0)
        );

        IOobject UHeader
        (
            "U",
            runTime.timeName(),
            mesh,
            IOobject::MUST_READ,
            IOobject::NO_WRITE
        );
        
        char flowRegime[30] = "";
        
        if (UHeader.typeHeaderOk<volScalarField>(false))
        {
            Info<< "Reading field U" << endl;
            volVectorField U(UHeader, mesh);


            if (laminar) {strcat(flowRegime,"laminar ");} else {strcat(flowRegime,"turbulent ");}
            if (!incompressible)
            {
                calcCompressibleYPlus(mesh, runTime, U, yPlus, laminar);
                strcat(flowRegime,"compressible");
            }
            else
            {
                calcIncompressibleYPlus(mesh, runTime, U, yPlus, laminar);
                strcat(flowRegime,"incompressible");
            }
        }
        else
        {
            Info<< "    no U field" << endl;
        }

        Info<< "\e[1;33m" << "Writing y+ to field " << yPlus.name() << ", " <<
        flowRegime << " flow \e[0m" << nl << endl;

        yPlus.write();
    }

    Info<< "End\n" << endl;

    return 0;
}


// ************************************************************************* //
