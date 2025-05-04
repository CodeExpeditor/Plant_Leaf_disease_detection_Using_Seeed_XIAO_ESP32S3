from flask import Flask, request, jsonify, Response
from functools import wraps
import bcrypt
import openai
import base64
import json
from dotenv import load_dotenv
import os
from pymongo import MongoClient
from flask_socketio import SocketIO, emit
from flask_cors import CORS, cross_origin
from bson import json_util


mongo_client = MongoClient("mongodb://localhost:27017")
db = mongo_client["leaf_guard"]
collection = db["captured_data"]

load_dotenv()

app = Flask(__name__)
app.config['SECRET_KEY'] = os.getenv("SECRET_KEY")
socketio = SocketIO(app, cors_allowed_origins="*")
app.config['CORS_HEADERS'] = 'Content-Type'
CORS(app, origins="*", methods=["GET", "POST"], supports_credentials=True)

key = os.getenv("OPEN_API_KEY")
openai.api_key = key


# def require_basic_auth(f):
#     @wraps(f)
#     def decorated(*args, **kwargs):
#         auth = request.authorization
#         if not auth:
#             return Response(
#                 "Missing credentials", 
#                 401, 
#                 {"WWW-Authenticate": 'Basic realm="Login Required"'}
#             )
#         user = users_collection.find_one({"username": auth.username})
#         if not user:
#             return Response(
#                 "Unauthorized", 
#                 401, 
#                 {"WWW-Authenticate": "Login Required"}
#             )
#         stored_password = user.get("password")
#         if isinstance(stored_password, bytes):

#             if not bcrypt.checkpw(auth.password.encode('utf-8'), stored_password):
#                 return Response(
#                     "Unauthorized", 
#                     401, 
#                     {"WWW-Authenticate": "Login Required"}
#                 )
#         else:
            
#             if auth.password != stored_password:
#                 return Response(
#                     "Unauthorized", 
#                     401, 
#                     {"WWW-Authenticate": "Login Required"}
#                 )
#         return f(*args, **kwargs)
#     return decorated


def encode_image(file_obj):
    return base64.b64encode(file_obj.read()).decode("utf-8")
    
@app.route('/analyze', methods=['POST'])
@cross_origin()
# @require_basic_auth
def analyze():
    if 'image' not in request.files:
        return jsonify({"error": "No image provided"}), 400

    image_file = request.files['image']
    base64_image = encode_image(image_file)

    prompt = (
        """I am sending an image of a leaf. Check whether the leaf is diseased or not. 
        If diseased then give me the treatment for the disease. Also consider the weather condition like temperature, humidity etc., 
        Give me the reason for the disease also and provide me the repsonse by following structure"""
    )
    
    client = openai.OpenAI(api_key=key)
    
    response = client.responses.create(
        model="gpt-4o",
        input=[
            {
                "role": "user",
                "content": [
                    {"type": "input_text", "text": prompt},
                    {"type": "input_image", "image_url": f"data:image/jpeg;base64,{base64_image}"}
                ]
            }
        ],
        text={
            "format": {
                "name":"Treatment",
                "type": "json_schema",
                "schema": {
                    "type": "object",
                    "strict": True,
                    "name": "Treatment",
                    "properties": {
                        "disease_name":{
                            "type": "string",
                            "description": "Name of the disease"
                        },
                        "treatment": {
                            "type": "string",
                            "description": "Treatment for the disease"
                        },
                        "reason":{
                            "type": "string",
                            "description": "Reason for the disease"
                        },
                        "decision":{
                            "type": "string",
                            "enum": ["Diseased", "Not Diseased"],
                            "description": "Decision on whether the leaf image indicates diseased or not."
                        }
                    },
                    "required": ["disease_name","treatment","decision","reason"],
                    "additionalProperties": False
                }
            }
        }
    )

    try:
        result = json.loads(response.output_text)
    except Exception as e:
        return jsonify({"error": "Failed to parse response", "details": str(e)}), 500
    
    result["base64_image"] = base64_image
    inserted_doc = collection.insert_one(result)
    
    # Convert the result to JSON serializable format
    emit_result = result.copy()
    emit_result['_id'] = str(inserted_doc.inserted_id)
    socketio.emit("result", emit_result)
    return jsonify(emit_result), 200

@app.route('/history', methods=['GET'])
@cross_origin()
def get_history():
    data = list(collection.find())
    serialized_data = json.loads(json_util.dumps(data))
    return jsonify(serialized_data)
    

if __name__ == '__main__':
    app.run(host="0.0.0.0",debug=True, port=5000)
